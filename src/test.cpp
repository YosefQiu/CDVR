// TransferFunctionTest.cpp
#include "test.h"
#include "PipelineManager.h"



TransferFunctionTest::TransferFunctionTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat)
    : m_device(device), m_queue(queue), m_swapChainFormat(swapChainFormat) {
    
    std::cout << "[TransferFunctionTest] === TransferFunctionTest Created ===" << std::endl;
}

TransferFunctionTest::~TransferFunctionTest() 
{
    m_computeStage.Release();
    m_renderStage.Release();
    if (m_outputTextureView) 
    {
        m_outputTextureView.release();
        m_outputTextureView = nullptr;
    }
    if (m_outputTexture) 
    {
        m_outputTexture.release();
        m_outputTexture = nullptr;
    }
}


bool TransferFunctionTest::Initialize(glm::mat4 vMat, glm::mat4 pMat) 
{
    std::cout << "[TransferFunctionTest] Initializing Transfer Function Test..." << std::endl;

    if (!InitDataFromBinary("./pruned_simple_data.bin")) return false;
    
    m_RS_Uniforms.viewMatrix = vMat;
    m_RS_Uniforms.projMatrix = pMat;

    if (!InitOutputTexture()) return false;
    if (!m_computeStage.Init(m_device, m_queue, m_sparsePoints, m_CS_Uniforms)) return false;
    if (!m_renderStage.Init(m_device, m_queue, m_RS_Uniforms, m_header.width, m_header.height)) return false;
    if (!m_renderStage.CreatePipeline(m_device, m_swapChainFormat)) return false;
    if (!m_renderStage.InitBindGroup(m_device, m_outputTextureView)) return false;
    
    std::cout << "[TransferFunctionTest] Transfer Function Test initialized successfully!" << std::endl;
    return true;

}

bool TransferFunctionTest::InitDataFromBinary(const std::string& filename) 
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&m_header), sizeof(DataHeader));
    
    std::cout << "[TransferFunctionTest] Loading sparse data:" << std::endl;
    std::cout << "[TransferFunctionTest]   Grid size: " << m_header.width << " x " << m_header.height << std::endl;
    std::cout << "[TransferFunctionTest]   Number of points: " << m_header.numPoints << std::endl;
    
    // 读取稀疏点数据
    m_sparsePoints.resize(m_header.numPoints);
    file.read(reinterpret_cast<char*>(m_sparsePoints.data()), 
              m_header.numPoints * sizeof(SparsePoint));
    
    file.close();

    // 2. 创建 Compute Uniform Buffer

    m_CS_Uniforms.gridWidth = static_cast<float>(m_header.width);
    m_CS_Uniforms.gridHeight = static_cast<float>(m_header.height);
    m_CS_Uniforms.numPoints = static_cast<uint32_t>(m_sparsePoints.size());
    m_CS_Uniforms.searchRadius = 5.0f;
    
    // 计算值的范围（用于颜色映射）
    ComputeValueRange();

    // TEST FOR KD-Tree
    KDTreeBuilder builder;
    auto kdTreeData = builder.buildKDTree(m_sparsePoints);
    if (kdTreeData.totalNodes == 0) {
        std::cerr << "[ERROR]::TransferFunctionTest: Failed to build KD-Tree" << std::endl;
        return false;
    }

    std::cout << "\n[TransferFunctionTest] Verifying KD-Tree..." << std::endl;
    auto verificationResult = KDTreeVerifier::verifyKDTree(kdTreeData, m_sparsePoints);
    
    if (!verificationResult.isValid) {
        std::cerr << "[ERROR] KD-Tree verification failed: " << verificationResult.errorMessage << std::endl;
        return false;
    }
    
    std::cout << "[TransferFunctionTest] KD-Tree verification passed!" << std::endl;
    
    return true;
}

void TransferFunctionTest::ComputeValueRange()
{
    float minValue = std::numeric_limits<float>::max();
    float maxValue = std::numeric_limits<float>::lowest();
    
    for (const auto& point : m_sparsePoints) 
    {
        minValue = std::min(minValue, point.value);
        maxValue = std::max(maxValue, point.value);
    }
 
    m_CS_Uniforms.minValue = minValue;
    m_CS_Uniforms.maxValue = maxValue;
    std::cout << "[TransferFunctionTest] Value range: [" << minValue << ", " << maxValue << "]" << std::endl;
}

void TransferFunctionTest::UpdateSSBO(wgpu::TextureView tfTextureView)
{
    m_tfTextureView = tfTextureView;
    if (m_tfTextureView) 
    {
        m_computeStage.UpdateBindGroup(m_device, m_tfTextureView, m_outputTextureView);
        m_needsUpdate = true; 
    }


    if (m_needsUpdate && m_computeStage.TF_bindGroup && m_computeStage.pipeline) 
    {
        m_needsUpdate = false;
        m_computeStage.RunCompute(m_device, m_queue);
        
        // 尝试更强制的同步方法
        #if defined(WEBGPU_BACKEND_DAWN)
        // Dawn: 多次tick确保完成
        for (int i = 0; i < 10; ++i) {
            m_device.tick();
        }
        #elif defined(WEBGPU_BACKEND_WGPU)
        // wgpu: 使用阻塞poll
        m_device.poll(true);  // true = 阻塞等待
        #endif
    }
}

bool TransferFunctionTest::InitOutputTexture(uint32_t width, uint32_t height, uint32_t depth, wgpu::TextureFormat format)
{
    
    // 1. 创建输出纹理（CS的结果）
    wgpu::TextureDescriptor outputTextureDesc = {};
    outputTextureDesc.label = "CS Output Texture";
    outputTextureDesc.dimension = wgpu::TextureDimension::_2D;
    outputTextureDesc.size = {width, height, depth};
    outputTextureDesc.format = format;
    outputTextureDesc.usage = wgpu::TextureUsage::StorageBinding |     // 计算着色器写入
                              wgpu::TextureUsage::TextureBinding;      // 渲染着色器读取
    outputTextureDesc.mipLevelCount = 1;
    outputTextureDesc.sampleCount = 1;
    outputTextureDesc.viewFormatCount = 0;
    outputTextureDesc.viewFormats = nullptr;
    
    m_outputTexture = m_device.createTexture(outputTextureDesc);
    if (!m_outputTexture) {
        std::cout << "[ERROR]::InitOutputTexture: Failed to create output texture" << std::endl;
        return false;
    }
    
    wgpu::TextureViewDescriptor outputViewDesc = {};
    outputViewDesc.label = "CS Output View";
    outputViewDesc.format = format;
    outputViewDesc.dimension = wgpu::TextureViewDimension::_2D;
    outputViewDesc.baseMipLevel = 0;
    outputViewDesc.mipLevelCount = 1;
    outputViewDesc.baseArrayLayer = 0;
    outputViewDesc.arrayLayerCount = 1;
    outputViewDesc.aspect = wgpu::TextureAspect::All;
    
    m_outputTextureView = m_outputTexture.createView(outputViewDesc);
    if (!m_outputTextureView) {
        std::cout << "[ERROR]::InitSSBO: Failed to create output texture view" << std::endl;
        return false;
    }

    return true;
}

void TransferFunctionTest::UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix)
{
    m_RS_Uniforms.viewMatrix = viewMatrix;
    m_RS_Uniforms.projMatrix = projMatrix;
    
   m_renderStage.UpdateUniforms(m_queue, m_RS_Uniforms);
}

bool TransferFunctionTest::ComputeStage::Init(wgpu::Device device, wgpu::Queue queue, const std::vector<SparsePoint>& sparsePoints, const CS_Uniforms uniforms)
{
    if (!InitSSBO(device, queue, sparsePoints)) return false;
    if (!InitUBO(device, uniforms)) return false;
    if (!CreatePipeline(device)) return false;
    return true;
}

bool TransferFunctionTest::ComputeStage::InitUBO(wgpu::Device device, CS_Uniforms uniforms) 
{
    // 1. 创建计算着色器的Uniform Buffer
    wgpu::BufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.label = "Compute Uniform Buffer";
    uniformBufferDesc.size = sizeof(CS_Uniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniformBufferDesc.mappedAtCreation = false;
    
    uniformBuffer = device.createBuffer(uniformBufferDesc);
    
    if (!uniformBuffer) {
        std::cout << "[ERROR]::InitUBO Failed to create uniform buffer" << std::endl;
        return false;
    }
    
    // 将Uniform数据写入缓冲区
    device.getQueue().writeBuffer(uniformBuffer, 0, &uniforms, sizeof(CS_Uniforms));

    return true;
}

bool TransferFunctionTest::ComputeStage::InitSSBO(wgpu::Device device, wgpu::Queue queue, const std::vector<SparsePoint>& sparsePoints) 
{
    if (sparsePoints.empty()) return false;

    // 1. 创建稀疏点数据的存储缓冲区
    wgpu::BufferDescriptor storageBufferDesc = {};
    storageBufferDesc.label = "Sparse Points Buffer";
    storageBufferDesc.size = sparsePoints.size() * sizeof(SparsePoint);
    storageBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    storageBufferDesc.mappedAtCreation = false;
    
    storageBuffer = device.createBuffer(storageBufferDesc);
    
    if (!storageBuffer) {
        std::cout << "[ERROR]::InitSSBO Failed to create storage buffer" << std::endl;
        return false;
    }
    
    // 将稀疏点数据写入缓冲区
    queue.writeBuffer(storageBuffer, 0, sparsePoints.data(), sparsePoints.size() * sizeof(SparsePoint));
    
    return true;
}


bool TransferFunctionTest::ComputeStage::CreatePipeline(wgpu::Device device) 
{
    
    auto& mgr = PipelineManager::getInstance();
    
    pipeline = mgr.createComputePipeline()
        .setDevice(device)
        .setLabel("Transfer Function Compute Pipeline")
        .setShader("../shaders/sparse_data.comp.wgsl", "main")
        .build();

    
    if (!pipeline) {
        std::cout << "[ERROR]::CreatePipline Failed to create compute pipeline!" << std::endl;
        return false;
    }
    
    std::cout << "[TransferFunctionTest] Compute pipeline created successfully" << std::endl;
    return true;
}

bool TransferFunctionTest::ComputeStage::UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputTexture) 
{
    if (!inputTF || !pipeline || !uniformBuffer || !storageBuffer) return false;  
    
    // 释放旧的绑定组
    if (data_bindGroup) 
    {
        data_bindGroup.release();
        data_bindGroup = nullptr;
    }
    if (TF_bindGroup) 
    {
        TF_bindGroup.release();
        TF_bindGroup = nullptr;
    }
    
    // 创建新的绑定组
    {
        wgpu::BindGroupEntry entries[3] = {};
        entries[0].binding = 0;
        entries[0].textureView = outputTexture;
        entries[1].binding = 1;
        entries[1].buffer = uniformBuffer;
        entries[1].offset = 0;
        entries[1].size = sizeof(CS_Uniforms);
        entries[2].binding = 2;
        entries[2].buffer = storageBuffer;
        entries[2].offset = 0;
        entries[2].size = WGPU_WHOLE_SIZE;

        wgpu::BindGroupDescriptor desc = {};
        desc.label = "Compute Data Bind Group";
        desc.layout = pipeline.getBindGroupLayout(0);
        desc.entryCount = 3;
        desc.entries = entries;
        
        data_bindGroup = device.createBindGroup(desc);
        if (!data_bindGroup)
        {
            std::cout << "[ERROR] ComputeStage: Failed to create data bind group" << std::endl;
            return false;
        }
    }

    {
        wgpu::BindGroupEntry entries[1] = {};
        entries[0].binding = 0;
        entries[0].textureView = inputTF;
        
        wgpu::BindGroupDescriptor desc = {};
        desc.label = "Compute TF Bind Group";
        desc.layout = pipeline.getBindGroupLayout(1);
        desc.entryCount = 1;
        desc.entries = entries;
        
        TF_bindGroup = device.createBindGroup(desc);
        if (!TF_bindGroup)
        {
            std::cout << "[ERROR] ComputeStage: Failed to create TF bind group" << std::endl;
            return false;
        }
    }
    
    return true;
}

void TransferFunctionTest::ComputeStage::RunCompute(wgpu::Device device, wgpu::Queue queue) 
{
     if (!data_bindGroup || !TF_bindGroup || !pipeline) return;
    
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "Compute Command Encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);
    
    wgpu::ComputePassDescriptor computePassDesc = {};
    computePassDesc.label = "Compute Pass";
    wgpu::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
    
    computePass.setPipeline(pipeline);
    computePass.setBindGroup(0, data_bindGroup, 0, nullptr);
    computePass.setBindGroup(1, TF_bindGroup, 0, nullptr); 
    computePass.dispatchWorkgroups((512 + 15) / 16, (512 + 15) / 16, 1);
    
    computePass.end();
    computePass.release();
    
    wgpu::CommandBufferDescriptor cmdBufferDesc = {};
    cmdBufferDesc.label = "Compute Command Buffer";
    wgpu::CommandBuffer commandBuffer = encoder.finish(cmdBufferDesc);
    encoder.release();
    
    queue.submit(1, &commandBuffer);
    commandBuffer.release();
}

void TransferFunctionTest::ComputeStage::Release() 
{
    if (pipeline) {
        pipeline.release();
        pipeline = nullptr;
    }
    if (data_bindGroup) {
        data_bindGroup.release();
        data_bindGroup = nullptr;
    }
    if (TF_bindGroup) {
        TF_bindGroup.release();
        TF_bindGroup = nullptr;
    }
}

bool TransferFunctionTest::RenderStage::Init(wgpu::Device device, wgpu::Queue queue, RS_Uniforms uniforms, float data_width, float data_height)
{
    if (!InitVBO(device, queue, data_width, data_height)) return false;
    if (!InitUBO(device, uniforms)) return false;
    if (!InitSampler(device)) return false;

    return true;
}

bool TransferFunctionTest::RenderStage::InitVBO(wgpu::Device device, wgpu::Queue queue, float data_width, float data_height)
{
    // float dataAspect = static_cast<float>(data_width) / static_cast<float>(data_height);  

    // 1. 创建全屏四边形顶点
    float vertices[] = {
        // posX, posY,              u,    v
        0.0f,        0.0f,         0.0f, 0.0f,  // 左下
        data_width,  0.0f,         1.0f, 0.0f,  // 右下
        0.0f,        data_height,  0.0f, 1.0f,  // 左上
        data_width,  data_height,  1.0f, 1.0f,  // 右上
    };
    
    wgpu::BufferDescriptor vertexBufferDesc = {};
    vertexBufferDesc.label = "Vertex Buffer";
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    vertexBufferDesc.mappedAtCreation = false;
    vertexBuffer = device.createBuffer(vertexBufferDesc);
    
    if (!vertexBuffer) {
        std::cout << "[ERROR]::InitVBO Failed to create vertex buffer" << std::endl;
        return false;
    }
    
    queue.writeBuffer(vertexBuffer, 0, vertices, sizeof(vertices));
    
    return true;
}

bool TransferFunctionTest::RenderStage::InitSampler(wgpu::Device device)
{
    // 1. 创建渲染采样器
    wgpu::SamplerDescriptor renderSamplerDesc = {};
    renderSamplerDesc.label = "Render Sampler";
    renderSamplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    renderSamplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    renderSamplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    renderSamplerDesc.magFilter = wgpu::FilterMode::Linear;
    renderSamplerDesc.minFilter = wgpu::FilterMode::Linear;
    renderSamplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    renderSamplerDesc.lodMinClamp = 0.0f;
    renderSamplerDesc.lodMaxClamp = 1.0f;
    renderSamplerDesc.maxAnisotropy = 1;
    sampler = device.createSampler(renderSamplerDesc);
    
    if (!sampler) {
        std::cout << "[ERROR]::InitSampler Failed to create render sampler" << std::endl;
        return false;
    }
    return true;
}

bool TransferFunctionTest::RenderStage::InitUBO(wgpu::Device device, RS_Uniforms uniforms)
{
    wgpu::BufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.size = sizeof(RS_Uniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniformBuffer = device.createBuffer(uniformBufferDesc);
    
    UpdateUniforms(device.getQueue(), uniforms);  // 初始化 uniform 数据
    return true;
}

void TransferFunctionTest::RenderStage::UpdateUniforms(wgpu::Queue queue, RS_Uniforms uniforms)
{
    queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(RS_Uniforms));
}


bool TransferFunctionTest::RenderStage::CreatePipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat)
{

    auto& mgr = PipelineManager::getInstance();
    
    pipeline = mgr.createRenderPipeline()
        .setDevice(device)
        .setLabel("Transfer Function Render Pipeline")
        .setVertexShader("../shaders/sparse_data.vert.wgsl", "main")
        .setFragmentShader("../shaders/sparse_data.frag.wgsl", "main")
        .setVertexLayout(VertexLayoutBuilder::createPositionTexCoord())
        .setSwapChainFormat(swapChainFormat)
        .setAlphaBlending()          // 使用预设的 Alpha 混合
        .setReadOnlyDepth()          // 使用预设的只读深度
        .build();


    if (!pipeline) {
        std::cout << "ERROR: Failed to create render pipeline!" << std::endl;
        return false;
    }

    return true;
}

bool TransferFunctionTest::RenderStage::InitBindGroup(wgpu::Device device, wgpu::TextureView outputTexture)
{
    if (!outputTexture || !pipeline || !uniformBuffer || !sampler) 
    {  
        std::cout << "[ERROR]::InitBindGroup: Missing prerequisites for bind group creation" << std::endl;
        std::cout << "outputTexture: " << (outputTexture ? "OK" : "NULL") << std::endl;
        std::cout << "pipeline: " << (pipeline ? "OK" : "NULL") << std::endl; 
        std::cout << "uniformBuffer: " << (uniformBuffer ? "OK" : "NULL") << std::endl;
        std::cout << "sampler: " << (sampler ? "OK" : "NULL") << std::endl;
        return false;
    }

    wgpu::BindGroupEntry renderBindGroupEntries[3] = {};
    renderBindGroupEntries[0].binding = 0;
    renderBindGroupEntries[0].buffer = uniformBuffer;
    renderBindGroupEntries[0].offset = 0;
    renderBindGroupEntries[0].size = sizeof(RS_Uniforms);
    renderBindGroupEntries[1].binding = 1;
    renderBindGroupEntries[1].textureView = outputTexture;
    renderBindGroupEntries[2].binding = 2;
    renderBindGroupEntries[2].sampler = sampler;
    
    wgpu::BindGroupDescriptor renderBindGroupDesc = {};
    renderBindGroupDesc.label = "Render Bind Group";
    renderBindGroupDesc.layout = pipeline.getBindGroupLayout(0);
    renderBindGroupDesc.entryCount = 3;
    renderBindGroupDesc.entries = renderBindGroupEntries;
    bindGroup = device.createBindGroup(renderBindGroupDesc);
    
    
    
    if (!bindGroup) {
        std::cout << "[ERROR]::InitBindGroup Failed to create render bind group!" << std::endl;
        return false;
    }
    
    std::cout << "[TransferFunctionTest] Render pipeline created successfully!" << std::endl;
    return true;
}

void TransferFunctionTest::RenderStage::Render(wgpu::RenderPassEncoder renderPass) 
{
    if (!pipeline || !bindGroup || !vertexBuffer) return;
    
    renderPass.setPipeline(pipeline);
    renderPass.setBindGroup(0, bindGroup, 0, nullptr); 
    renderPass.setVertexBuffer(0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
    renderPass.draw(4, 1, 0, 0); 

}

void TransferFunctionTest::RenderStage::Release() 
{
    if (pipeline) {
        pipeline.release();
        pipeline = nullptr;
    }
    if (bindGroup) {
        bindGroup.release();
        bindGroup = nullptr;
    }
    if (sampler) {
        sampler.release();
        sampler = nullptr;
    }
    if (vertexBuffer) {
        vertexBuffer.release();
        vertexBuffer = nullptr;
    }
}


void TransferFunctionTest::Render(wgpu::RenderPassEncoder renderPass) 
{
    m_renderStage.Render(renderPass);
}



void TransferFunctionTest::OnWindowResize(glm::mat4 veiwMatrix, glm::mat4 projMatrix) 
{
    UpdateUniforms(veiwMatrix, projMatrix);
}