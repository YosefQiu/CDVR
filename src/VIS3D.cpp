#include "VIS3D.h"
#include "PipelineManager.h"


VIS3D::VIS3D(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat)
    : m_device(device), m_queue(queue), m_swapChainFormat(swapChainFormat) {
    
    std::cout << "[VIS3D] === VIS3D Created ===" << std::endl;
}

VIS3D::~VIS3D() 
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

bool VIS3D::Initialize(glm::mat4 vMat, glm::mat4 pMat) 
{
    std::cout << "[VIS3D] Initializing Transfer Function 3D Test..." << std::endl;

    if (!InitDataFromBinary("./pruned_simple_data.bin")) return false;
    
    m_RS_Uniforms.viewMatrix = vMat;
    m_RS_Uniforms.projMatrix = pMat;
    m_RS_Uniforms.modelMatrix = glm::mat4(1.0f);

    if (!InitOutputTexture()) return false;
    if (!m_computeStage.Init(m_device, m_queue, m_sparsePoints, m_KDTreeData, m_CS_Uniforms)) return false;
    if (!m_renderStage.Init(m_device, m_queue, m_RS_Uniforms, m_header.width, m_header.height, m_header.depth)) return false;
    if (!m_renderStage.CreatePipeline(m_device, m_swapChainFormat)) return false;
    if (!m_renderStage.InitBindGroup(m_device, m_outputTextureView)) return false;
    
    std::cout << "[VIS3D] Transfer Function 3D Test initialized successfully!" << std::endl;
    return true;
}

bool VIS3D::InitDataFromBinary(const std::string& filename) 
{
    // std::ifstream file(filename, std::ios::binary);
    // if (!file.is_open()) {
    //     std::cerr << "Failed to open file: " << filename << std::endl;
    //     return false;
    // }

    // // 读取头部信息 (假设2D数据，我们扩展为3D)
    // DataHeader header2D;
    // file.read(reinterpret_cast<char*>(&header2D), sizeof(DataHeader) - sizeof(uint32_t)); // 不包含depth
    
    // m_header.width = header2D.width;
    // m_header.height = header2D.height;
    // m_header.depth = std::max(header2D.width, header2D.height) / 2; // 简单设置深度
    // m_header.numPoints = header2D.numPoints;
    
    // std::cout << "[VIS3D] Loading sparse data:" << std::endl;
    // std::cout << "[VIS3D]   Grid size: " << m_header.width << " x " << m_header.height << " x " << m_header.depth << std::endl;
    // std::cout << "[VIS3D]   Number of points: " << m_header.numPoints << std::endl;
    
    // // 读取稀疏点数据
    // m_sparsePoints.resize(m_header.numPoints);
    // file.read(reinterpret_cast<char*>(m_sparsePoints.data()), 
    //           m_header.numPoints * sizeof(SparsePoint));
    
    // file.close();

    // // 设置计算着色器的uniform参数
    // m_CS_Uniforms.gridWidth = static_cast<float>(m_header.width);
    // m_CS_Uniforms.gridHeight = static_cast<float>(m_header.height);
    // m_CS_Uniforms.gridDepth = static_cast<float>(m_header.depth);
    // m_CS_Uniforms.searchRadius = std::ceil(std::sqrt(m_CS_Uniforms.gridWidth * m_CS_Uniforms.gridWidth + 
    //                                                  m_CS_Uniforms.gridHeight * m_CS_Uniforms.gridHeight +
    //                                                  m_CS_Uniforms.gridDepth * m_CS_Uniforms.gridDepth));
    
    // // 计算值的范围
    // ComputeValueRange();

    // // 构建KD-Tree
    // KDTreeBuilder builder;
    // if (builder.buildTree(m_sparsePoints)) 
    // {
    //     m_KDTreeData.points = builder.getGPUPoints();
    //     m_KDTreeData.numLevels = builder.getNumLevels();
    // }
    // else 
    // {
    //     std::cerr << "[ERROR]::VIS3D: Failed to build KD-Tree" << std::endl;
    //     return false;
    // }
 
    // std::cout << "[VIS3D]   Total points: " << m_KDTreeData.points.size() << std::endl;
    // std::cout << "[VIS3D]   Number of levels: " << m_KDTreeData.numLevels << std::endl;

    // m_CS_Uniforms.totalNodes = m_KDTreeData.points.size();
    // m_CS_Uniforms.numLevels = m_KDTreeData.numLevels;
    // m_CS_Uniforms.interpolationMethod = 0; 

    // return true;

    // for TEST
    // 不读取文件，直接生成假数据
    std::cout << "[VIS3D] Generating fake data (no real 3D data needed)..." << std::endl;
    
    // 设置假的头部信息
    m_header.width = 128;
    m_header.height = 128;
    m_header.depth = 128;
    m_header.numPoints = 10;  // 随便几个点就行
    
    std::cout << "[VIS3D] Fake data:" << std::endl;
    std::cout << "[VIS3D]   Grid size: " << m_header.width << " x " << m_header.height << " x " << m_header.depth << std::endl;
    std::cout << "[VIS3D]   Number of points: " << m_header.numPoints << std::endl;
    
    // 生成几个假的稀疏点
    m_sparsePoints.clear();
    m_sparsePoints.resize(m_header.numPoints);
    for (int i = 0; i < m_header.numPoints; i++) {
        m_sparsePoints[i].x = i * 10.0f;
        m_sparsePoints[i].y = i * 10.0f;
        m_sparsePoints[i].value = i * 0.1f;
        m_sparsePoints[i].padding = 0.0f;
    }

    // 设置计算着色器的uniform参数
    m_CS_Uniforms.gridWidth = static_cast<float>(m_header.width);
    m_CS_Uniforms.gridHeight = static_cast<float>(m_header.height);
    m_CS_Uniforms.gridDepth = static_cast<float>(m_header.depth);
    m_CS_Uniforms.searchRadius = 50.0f;  // 随便一个值
    
    // 计算值的范围
    ComputeValueRange();

    // 生成假的KD-Tree数据
    m_KDTreeData.points.clear();
    m_KDTreeData.points.resize(m_header.numPoints);
    for (int i = 0; i < m_header.numPoints; i++) {
        m_KDTreeData.points[i].x = m_sparsePoints[i].x;
        m_KDTreeData.points[i].y = m_sparsePoints[i].y;
        m_KDTreeData.points[i].value = m_sparsePoints[i].value;
        m_KDTreeData.points[i].padding = 0.0f;
    }
    m_KDTreeData.numLevels = 3;  // 随便一个值
    
    std::cout << "[VIS3D]   Total points: " << m_KDTreeData.points.size() << std::endl;
    std::cout << "[VIS3D]   Number of levels: " << m_KDTreeData.numLevels << std::endl;

    m_CS_Uniforms.totalNodes = m_KDTreeData.points.size();
    m_CS_Uniforms.numLevels = m_KDTreeData.numLevels;
    m_CS_Uniforms.interpolationMethod = 0; 

    return true;
}

void VIS3D::ComputeValueRange()
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
    std::cout << "[VIS3D] Value range: [" << minValue << ", " << maxValue << "]" << std::endl;
}

void VIS3D::UpdateSSBO(wgpu::TextureView tfTextureView)
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
        
        #if defined(WEBGPU_BACKEND_DAWN)
        for (int i = 0; i < 10; ++i) {
            m_device.tick();
        }
        #elif defined(WEBGPU_BACKEND_WGPU)
        m_device.poll(true);
        #endif
    }
}

bool VIS3D::InitOutputTexture(uint32_t width, uint32_t height, uint32_t depth, wgpu::TextureFormat format)
{
    // 创建3D输出纹理
    wgpu::TextureDescriptor outputTextureDesc = {};
    outputTextureDesc.label = "CS 3D Output Texture";
    outputTextureDesc.dimension = wgpu::TextureDimension::_3D;
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
        std::cout << "[ERROR]::InitOutputTexture: Failed to create 3D output texture" << std::endl;
        return false;
    }
    
    wgpu::TextureViewDescriptor outputViewDesc = {};
    outputViewDesc.label = "CS 3D Output View";
    outputViewDesc.format = format;
    outputViewDesc.dimension = wgpu::TextureViewDimension::_3D;
    outputViewDesc.baseMipLevel = 0;
    outputViewDesc.mipLevelCount = 1;
    outputViewDesc.baseArrayLayer = 0;
    outputViewDesc.arrayLayerCount = 1;
    outputViewDesc.aspect = wgpu::TextureAspect::All;
    
    m_outputTextureView = m_outputTexture.createView(outputViewDesc);
    if (!m_outputTextureView) {
        std::cout << "[ERROR]::InitOutputTexture: Failed to create 3D output texture view" << std::endl;
        return false;
    }

    return true;
}

void VIS3D::UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix)
{
    m_RS_Uniforms.viewMatrix = viewMatrix;
    m_RS_Uniforms.projMatrix = projMatrix;
    
    m_renderStage.UpdateUniforms(m_queue, m_RS_Uniforms);
}

void VIS3D::SetModelMatrix(glm::mat4 modelMatrix)
{
    m_RS_Uniforms.modelMatrix = modelMatrix;
    m_renderStage.UpdateUniforms(m_queue, m_RS_Uniforms);
}

void VIS3D::SetInterpolationMethod(int kValue)
{
    if (m_CS_Uniforms.interpolationMethod != (uint32_t)kValue) 
    {
        m_CS_Uniforms.interpolationMethod = kValue;
        m_queue.writeBuffer(m_computeStage.uniformBuffer, 0, &m_CS_Uniforms, sizeof(CS_Uniforms));
        m_needsUpdate = true;
    }
}

void VIS3D::SetSearchRadius(float radius)
{
    if (m_CS_Uniforms.searchRadius != radius) 
    {
        m_CS_Uniforms.searchRadius = radius;
        m_queue.writeBuffer(m_computeStage.uniformBuffer, 0, &m_CS_Uniforms, sizeof(CS_Uniforms));
        m_needsUpdate = true;
    }
}

// ComputeStage 实现
bool VIS3D::ComputeStage::Init(wgpu::Device device, wgpu::Queue queue, 
    const std::vector<SparsePoint>& sparsePoints, 
    const KDTreeBuilder::TreeData& kdTreeData,
    const CS_Uniforms uniforms)
{
    if (!InitSSBO(device, queue, sparsePoints)) return false;
    if (!InitUBO(device, uniforms)) return false;
    if (!InitKDTreeBuffers(device, queue, kdTreeData)) return false;
    if (!CreatePipeline(device)) return false;
    return true;
}

bool VIS3D::ComputeStage::InitUBO(wgpu::Device device, CS_Uniforms uniforms) 
{
    wgpu::BufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.label = "Compute 3D Uniform Buffer";
    uniformBufferDesc.size = sizeof(CS_Uniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniformBufferDesc.mappedAtCreation = false;
    
    uniformBuffer = device.createBuffer(uniformBufferDesc);
    
    if (!uniformBuffer) {
        std::cout << "[ERROR]::InitUBO Failed to create uniform buffer" << std::endl;
        return false;
    }
    
    device.getQueue().writeBuffer(uniformBuffer, 0, &uniforms, sizeof(CS_Uniforms));
    return true;
}

bool VIS3D::ComputeStage::InitSSBO(wgpu::Device device, wgpu::Queue queue, const std::vector<SparsePoint>& sparsePoints) 
{
    if (sparsePoints.empty()) return false;

    wgpu::BufferDescriptor storageBufferDesc = {};
    storageBufferDesc.label = "Sparse Points 3D Buffer";
    storageBufferDesc.size = sparsePoints.size() * sizeof(SparsePoint);
    storageBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    storageBufferDesc.mappedAtCreation = false;
    
    storageBuffer = device.createBuffer(storageBufferDesc);
    
    if (!storageBuffer) {
        std::cout << "[ERROR]::InitSSBO Failed to create storage buffer" << std::endl;
        return false;
    }
    
    queue.writeBuffer(storageBuffer, 0, sparsePoints.data(), sparsePoints.size() * sizeof(SparsePoint));
    return true;
}

bool VIS3D::ComputeStage::InitKDTreeBuffers(wgpu::Device device, wgpu::Queue queue, 
    const KDTreeBuilder::TreeData& kdTreeData)
{
    if (kdTreeData.points.empty()) {
        std::cout << "[ERROR]::InitKDTreeBuffers KD-Tree data is empty" << std::endl;
        return false;
    }

    wgpu::BufferDescriptor kdNodesBufferDesc = {};
    kdNodesBufferDesc.label = "KD-Tree 3D Points Buffer";
    kdNodesBufferDesc.size = kdTreeData.points.size() * sizeof(GPUPoint);
    kdNodesBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    kdNodesBufferDesc.mappedAtCreation = false;
    
    kdNodesBuffer = device.createBuffer(kdNodesBufferDesc);
    
    if (!kdNodesBuffer) {
        std::cout << "[ERROR]::InitKDTreeBuffers Failed to create KD-Tree nodes buffer" << std::endl;
        return false;
    }
    
    queue.writeBuffer(kdNodesBuffer, 0, kdTreeData.points.data(), kdTreeData.points.size() * sizeof(GPUPoint));
    return kdNodesBuffer != nullptr;
}

bool VIS3D::ComputeStage::CreatePipeline(wgpu::Device device) {
    // Group 0: Output texture + Uniforms + Sparse points
    wgpu::BindGroupLayoutEntry group0Entries[3] = {};
    group0Entries[0].binding = 0;
    group0Entries[0].visibility = wgpu::ShaderStage::Compute;
    group0Entries[0].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
    group0Entries[0].storageTexture.format = wgpu::TextureFormat::RGBA16Float;
    group0Entries[0].storageTexture.viewDimension = wgpu::TextureViewDimension::_3D;
    
    group0Entries[1].binding = 1;
    group0Entries[1].visibility = wgpu::ShaderStage::Compute;
    group0Entries[1].buffer.type = wgpu::BufferBindingType::Uniform;
    
    group0Entries[2].binding = 2;
    group0Entries[2].visibility = wgpu::ShaderStage::Compute;
    group0Entries[2].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    
    wgpu::BindGroupLayoutDescriptor group0Desc = {};
    group0Desc.label = "Group 0 3D Layout";
    group0Desc.entryCount = 3;
    group0Desc.entries = group0Entries;
    auto group0Layout = device.createBindGroupLayout(group0Desc);
    
    // Group 1: Transfer Function texture
    wgpu::BindGroupLayoutEntry group1Entries[1] = {};
    group1Entries[0].binding = 0;
    group1Entries[0].visibility = wgpu::ShaderStage::Compute;
    group1Entries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    group1Entries[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;
    
    wgpu::BindGroupLayoutDescriptor group1Desc = {};
    group1Desc.label = "Group 1 3D Layout";
    group1Desc.entryCount = 1;
    group1Desc.entries = group1Entries;
    auto group1Layout = device.createBindGroupLayout(group1Desc);
    
    // Group 2: KD-Tree data
    wgpu::BindGroupLayoutEntry group2Entries[1] = {};
    group2Entries[0].binding = 0;
    group2Entries[0].visibility = wgpu::ShaderStage::Compute;
    group2Entries[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    group2Entries[0].buffer.hasDynamicOffset = false;
    
    wgpu::BindGroupLayoutDescriptor group2Desc = {};
    group2Desc.label = "Group 2 3D Layout";
    group2Desc.entryCount = 1;
    group2Desc.entries = group2Entries;
    auto group2Layout = device.createBindGroupLayout(group2Desc);
    
    auto& mgr = PipelineManager::getInstance();
    pipeline = mgr.createComputePipeline()
        .setDevice(device)
        .setLabel("Transfer Function 3D Compute Pipeline")
        .setShader("../shaders/volume_simple.comp.wgsl", "main")
        .setExplicitLayout(true)
        .addBindGroupLayout(group0Layout)
        .addBindGroupLayout(group1Layout)
        .addBindGroupLayout(group2Layout)
        .build();
    
    group0Layout.release();
    group1Layout.release();
    group2Layout.release();

    if (!pipeline) {
        std::cout << "[ERROR] Failed to create 3D compute pipeline!" << std::endl;
        return false;
    }
    
    std::cout << "[VIS3D] Compute pipeline created successfully" << std::endl;
    return true;
}

bool VIS3D::ComputeStage::UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputTexture) 
{
    if (!inputTF || !pipeline || !uniformBuffer || !storageBuffer) return false;  
    
    // 释放旧的绑定组
    if (data_bindGroup) {
        data_bindGroup.release();
        data_bindGroup = nullptr;
    }
    if (TF_bindGroup) {
        TF_bindGroup.release();
        TF_bindGroup = nullptr;
    }
    if (KDTree_bindGroup) {
        KDTree_bindGroup.release();
        KDTree_bindGroup = nullptr;
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
        desc.label = "Compute 3D Data Bind Group";
        desc.layout = pipeline.getBindGroupLayout(0);
        desc.entryCount = 3;
        desc.entries = entries;
        
        data_bindGroup = device.createBindGroup(desc);
        if (!data_bindGroup) {
            std::cout << "[ERROR] ComputeStage: Failed to create 3D data bind group" << std::endl;
            return false;
        }
    }

    {
        wgpu::BindGroupEntry entries[1] = {};
        entries[0].binding = 0;
        entries[0].textureView = inputTF;
        
        wgpu::BindGroupDescriptor desc = {};
        desc.label = "Compute 3D TF Bind Group";
        desc.layout = pipeline.getBindGroupLayout(1);
        desc.entryCount = 1;
        desc.entries = entries;
        
        TF_bindGroup = device.createBindGroup(desc);
        if (!TF_bindGroup) {
            std::cout << "[ERROR] ComputeStage: Failed to create 3D TF bind group" << std::endl;
            return false;
        }
    }

    {
        wgpu::BindGroupEntry entries[1] = {};
        entries[0].binding = 0;
        entries[0].buffer = kdNodesBuffer;
        entries[0].offset = 0;
        entries[0].size = WGPU_WHOLE_SIZE;

        wgpu::BindGroupDescriptor desc = {};
        desc.label = "Compute 3D KDTree Bind Group";
        desc.layout = pipeline.getBindGroupLayout(2);
        desc.entryCount = 1;
        desc.entries = entries;
        
        KDTree_bindGroup = device.createBindGroup(desc);
        if (!KDTree_bindGroup) {
            std::cout << "[ERROR] ComputeStage: Failed to create 3D KD-Tree bind group" << std::endl;
            return false;
        }
    }
    
    return true;
}

void VIS3D::ComputeStage::RunCompute(wgpu::Device device, wgpu::Queue queue) 
{
    if (!data_bindGroup || !TF_bindGroup || !KDTree_bindGroup || !pipeline) return;

    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "Compute 3D Command Encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);
    
    wgpu::ComputePassDescriptor computePassDesc = {};
    computePassDesc.label = "Compute 3D Pass";
    wgpu::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
    
    computePass.setPipeline(pipeline);
    computePass.setBindGroup(0, data_bindGroup, 0, nullptr);
    computePass.setBindGroup(1, TF_bindGroup, 0, nullptr); 
    computePass.setBindGroup(2, KDTree_bindGroup, 0, nullptr);
    computePass.dispatchWorkgroups((128 + 3) / 4, (128 + 3) / 4, (128 + 3) / 4);
    
    computePass.end();
    computePass.release();
    
    wgpu::CommandBufferDescriptor cmdBufferDesc = {};
    cmdBufferDesc.label = "Compute 3D Command Buffer";
    wgpu::CommandBuffer commandBuffer = encoder.finish(cmdBufferDesc);
    encoder.release();
    
    queue.submit(1, &commandBuffer);
    commandBuffer.release();
}

void VIS3D::ComputeStage::Release() 
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
    if (KDTree_bindGroup) {
        KDTree_bindGroup.release();
        KDTree_bindGroup = nullptr;
    }
    if (uniformBuffer) {
        uniformBuffer.release();
        uniformBuffer = nullptr;
    }
    if (storageBuffer) {
        storageBuffer.release();
        storageBuffer = nullptr;
    }
    if (kdNodesBuffer) {
        kdNodesBuffer.release();
        kdNodesBuffer = nullptr;
    }
}

// RenderStage 实现
bool VIS3D::RenderStage::Init(wgpu::Device device, wgpu::Queue queue, RS_Uniforms uniforms, float data_width, float data_height, float data_depth)
{
    if (!InitVBO(device, queue, data_width, data_height, data_depth)) return false;
    if (!InitUBO(device, uniforms)) return false;
    if (!InitSampler(device)) return false;
    return true;
}

bool VIS3D::RenderStage::InitVBO(wgpu::Device device, wgpu::Queue queue, float data_width, float data_height, float data_depth)
{
    // 立方体顶点数据 (位置 + 3D纹理坐标)
    float vertices[] = {
        // 前面 (z = 0.5)
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
        // 后面 (z = -0.5)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
    };
    
    // 立方体索引数据 (12个三角形，36个顶点)
    uint16_t indices[] = {
        // 前面
        0, 1, 2,  2, 3, 0,
        // 后面
        4, 6, 5,  6, 4, 7,
        // 左面
        4, 0, 3,  3, 7, 4,
        // 右面
        1, 5, 6,  6, 2, 1,
        // 上面
        3, 2, 6,  6, 7, 3,
        // 下面
        4, 5, 1,  1, 0, 4,
    };
    
    indexCount = sizeof(indices) / sizeof(uint16_t);
    
    // 创建顶点缓冲区
    wgpu::BufferDescriptor vertexBufferDesc = {};
    vertexBufferDesc.label = "3D Vertex Buffer";
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    vertexBufferDesc.mappedAtCreation = false;
    vertexBuffer = device.createBuffer(vertexBufferDesc);
    
    if (!vertexBuffer) {
        std::cout << "[ERROR]::InitVBO Failed to create 3D vertex buffer" << std::endl;
        return false;
    }
    
    queue.writeBuffer(vertexBuffer, 0, vertices, sizeof(vertices));
    
    // 创建索引缓冲区
    wgpu::BufferDescriptor indexBufferDesc = {};
    indexBufferDesc.label = "3D Index Buffer";
    indexBufferDesc.size = sizeof(indices);
    indexBufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
    indexBufferDesc.mappedAtCreation = false;
    indexBuffer = device.createBuffer(indexBufferDesc);
    
    if (!indexBuffer) {
        std::cout << "[ERROR]::InitVBO Failed to create 3D index buffer" << std::endl;
        return false;
    }
    
    queue.writeBuffer(indexBuffer, 0, indices, sizeof(indices));
    
    return true;
}

bool VIS3D::RenderStage::InitSampler(wgpu::Device device)
{
    wgpu::SamplerDescriptor renderSamplerDesc = {};
    renderSamplerDesc.label = "3D Render Sampler";
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
        std::cout << "[ERROR]::InitSampler Failed to create 3D render sampler" << std::endl;
        return false;
    }
    return true;
}

bool VIS3D::RenderStage::InitUBO(wgpu::Device device, RS_Uniforms uniforms)
{
    wgpu::BufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.label = "3D Render Uniform Buffer";
    uniformBufferDesc.size = sizeof(RS_Uniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniformBuffer = device.createBuffer(uniformBufferDesc);
    
    UpdateUniforms(device.getQueue(), uniforms);
    return true;
}

void VIS3D::RenderStage::UpdateUniforms(wgpu::Queue queue, RS_Uniforms uniforms)
{
    queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(RS_Uniforms));
}

bool VIS3D::RenderStage::CreatePipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat)
{
    auto& mgr = PipelineManager::getInstance();
    
    pipeline = mgr.createRenderPipeline()
        .setDevice(device)
        .setLabel("Transfer Function 3D Render Pipeline")
        .setVertexShader("../shaders/volume_raycasting.vert.wgsl", "main")
        .setFragmentShader("../shaders/volume_raycasting.frag.wgsl", "main")
        .setVertexLayout(VertexLayoutBuilder::createPositionTexCoord3D())  // 需要3D纹理坐标
        .setSwapChainFormat(swapChainFormat)
        .setAlphaBlending()
        .setVolumeRenderingDepth()  // 3D需要深度测试
        .build();

    if (!pipeline) {
        std::cout << "ERROR: Failed to create 3D render pipeline!" << std::endl;
        return false;
    }

    return true;
}

bool VIS3D::RenderStage::InitBindGroup(wgpu::Device device, wgpu::TextureView outputTexture)
{
    if (!outputTexture || !pipeline || !uniformBuffer || !sampler) 
    {  
        std::cout << "[ERROR]::InitBindGroup: Missing prerequisites for 3D bind group creation" << std::endl;
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
    renderBindGroupDesc.label = "3D Render Bind Group";
    renderBindGroupDesc.layout = pipeline.getBindGroupLayout(0);
    renderBindGroupDesc.entryCount = 3;
    renderBindGroupDesc.entries = renderBindGroupEntries;
    bindGroup = device.createBindGroup(renderBindGroupDesc);
    
    if (!bindGroup) {
        std::cout << "[ERROR]::InitBindGroup Failed to create 3D render bind group!" << std::endl;
        return false;
    }
    
    std::cout << "[VIS3D] Render pipeline created successfully!" << std::endl;
    return true;
}

void VIS3D::RenderStage::Render(wgpu::RenderPassEncoder renderPass) 
{
    if (!pipeline || !bindGroup || !vertexBuffer || !indexBuffer) return;
    
    renderPass.setPipeline(pipeline);
    renderPass.setBindGroup(0, bindGroup, 0, nullptr); 
    renderPass.setVertexBuffer(0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
    renderPass.setIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, WGPU_WHOLE_SIZE);
    renderPass.drawIndexed(indexCount, 1, 0, 0, 0);
}

void VIS3D::RenderStage::Release() 
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
    if (indexBuffer) {
        indexBuffer.release();
        indexBuffer = nullptr;
    }
    if (uniformBuffer) {
        uniformBuffer.release();
        uniformBuffer = nullptr;
    }
}

// 主要接口实现
void VIS3D::Render(wgpu::RenderPassEncoder renderPass) 
{
    m_renderStage.Render(renderPass);
}

void VIS3D::OnWindowResize(glm::mat4 viewMatrix, glm::mat4 projMatrix) 
{
    UpdateUniforms(viewMatrix, projMatrix);
}