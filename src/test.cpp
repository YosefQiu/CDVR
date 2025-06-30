// TransferFunctionTest.cpp
#include "test.h"
#include <fstream>
#include <webgpu/webgpu.hpp>


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

    if (!InitSSBO()) return false;
    if (!m_computeStage.CreatePipeline(m_device)) return false;
    if (!m_renderStage.Init(m_device, m_queue, m_RS_Uniforms)) return false;
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

bool TransferFunctionTest::InitSSBO()
{
    
    // 1. 创建输出纹理（CS的结果）
    wgpu::TextureDescriptor outputTextureDesc = {};
    outputTextureDesc.label = "CS Output Texture";
    outputTextureDesc.dimension = wgpu::TextureDimension::_2D;
    outputTextureDesc.size = {512, 512, 1};
    outputTextureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
     outputTextureDesc.usage = wgpu::TextureUsage::StorageBinding |     // 计算着色器写入
                              wgpu::TextureUsage::TextureBinding;      // 渲染着色器读取
    outputTextureDesc.mipLevelCount = 1;
    outputTextureDesc.sampleCount = 1;
    outputTextureDesc.viewFormatCount = 0;
    outputTextureDesc.viewFormats = nullptr;
    
    m_outputTexture = m_device.createTexture(outputTextureDesc);
    if (!m_outputTexture) {
        std::cout << "[ERROR]::InitSSBO: Failed to create output texture" << std::endl;
        return false;
    }
    
    wgpu::TextureViewDescriptor outputViewDesc = {};
    outputViewDesc.label = "CS Output View";
    outputViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
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

bool TransferFunctionTest::ComputeStage::CreatePipeline(wgpu::Device device) 
{

    const char* computeShaderSource = R"(
        @group(0) @binding(0) var outputTexture: texture_storage_2d<rgba8unorm, write>;
        @group(1) @binding(0) var inputTF: texture_2d<f32>;
        
        
        @compute @workgroup_size(16, 16)
        fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
            let texSize = textureDimensions(outputTexture);
            let coord = vec2<i32>(i32(global_id.x), i32(global_id.y));
            
            if (coord.x >= i32(texSize.x) || coord.y >= i32(texSize.y)) {
                return;
            }
            
            // 将像素坐标归一化到[0,1]
            let uv = vec2<f32>(f32(coord.x) / f32(texSize.x), f32(coord.y) / f32(texSize.y));
            
            // 简单的水平颜色映射：X坐标对应传输函数
            let tfX = uv.x;
            
            // 计算传输函数纹理中的像素坐标
            let tfTexSize = textureDimensions(inputTF);
            let tfPixelX = clamp(i32(tfX * f32(tfTexSize.x)), 0, i32(tfTexSize.x) - 1);
            let tfPixelCoord = vec2<i32>(tfPixelX, 0);
            
            // 直接读取传输函数颜色
            let color = textureLoad(inputTF, tfPixelCoord, 0);
            
            // 直接输出
            textureStore(outputTexture, coord, color);
        }
    )";
    
    wgpu::ShaderModuleWGSLDescriptor csWGSLDesc = {};
    csWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    csWGSLDesc.chain.next = nullptr;
    csWGSLDesc.code = computeShaderSource;

    wgpu::ShaderModuleDescriptor csDesc = {};
    csDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&csWGSLDesc);
    csDesc.label = "Compute Shader";
    wgpu::ShaderModule computeShader = device.createShaderModule(csDesc);

    if (!computeShader) {
        std::cout << "[ERROR]::CreatePipline Failed to create compute shader module!" << std::endl;
        return false;
    }
    
    // 创建计算管线
    wgpu::ComputePipelineDescriptor computePipelineDesc = {};
    computePipelineDesc.label = "Compute Pipeline";
    computePipelineDesc.compute.module = computeShader;
    computePipelineDesc.compute.entryPoint = "main";
    pipeline = device.createComputePipeline(computePipelineDesc);
    
    computeShader.release();
    
    if (!pipeline) {
        std::cout << "[ERROR]::CreatePipline Failed to create compute pipeline!" << std::endl;
        return false;
    }
    
    std::cout << "[TransferFunctionTest] Compute pipeline created successfully" << std::endl;
    return true;
}

bool TransferFunctionTest::ComputeStage::UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputTexture) 
{
    if (!inputTF || !pipeline) return false;
    
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
        wgpu::BindGroupEntry entries[1] = {};
        entries[0].binding = 0;
        entries[0].textureView = outputTexture;
        
        wgpu::BindGroupDescriptor desc = {};
        desc.label = "Compute Data Bind Group";
        desc.layout = pipeline.getBindGroupLayout(0);
        desc.entryCount = 1;
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

bool TransferFunctionTest::RenderStage::Init(wgpu::Device device, wgpu::Queue queue, RS_Uniforms uniforms)
{
    if (!InitVBO(device, queue, 150.0, 450.0)) return false;
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
    // Vertex Shader
    const char* vertexShaderSource = R"(
        struct VertexInput {
            @location(0) position: vec2<f32>,
            @location(1) texCoord: vec2<f32>,
        }
        
        struct Uniforms {
            viewMatrix: mat4x4<f32>,
            projMatrix: mat4x4<f32>,
        };

        struct VertexOutput {
            @builtin(position) position: vec4<f32>,
            @location(0) texCoord: vec2<f32>,
        }
        
        @group(0) @binding(0) var<uniform> uniforms: Uniforms;
        
        @vertex
        fn main(input: VertexInput) -> VertexOutput {
            var output: VertexOutput;
            let world_pos = vec4<f32>(input.position, 0.0, 1.0);
            output.position = uniforms.projMatrix * uniforms.viewMatrix * world_pos;
            output.texCoord = input.texCoord;
            return output;
        }
    )";
    
    // Fragment Shader
    const char* fragmentShaderSource = R"(

        @group(0) @binding(1) var dataTexture: texture_2d<f32>;
        @group(0) @binding(2) var dataSampler: sampler;

        struct FragmentInput {
            @location(0) uv: vec2<f32>,
        }
        
        @fragment
        fn main(input: FragmentInput) -> @location(0) vec4<f32> {
            let sampledColor = textureSample(dataTexture, dataSampler, input.uv);
            return sampledColor;  // 显示实际采样结果
        }
    )";
    
    std::cout << "[TransferFunctionTest] Creating render pipeline..." << std::endl;
    
    // 创建着色器模块
    wgpu::ShaderModuleWGSLDescriptor vsWGSLDesc = {};
    vsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    vsWGSLDesc.chain.next = nullptr;
    vsWGSLDesc.code = vertexShaderSource;

    wgpu::ShaderModuleDescriptor vsDesc = {};
    vsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&vsWGSLDesc);
    vsDesc.label = "Vertex Shader";
    wgpu::ShaderModule vertexShader = device.createShaderModule(vsDesc);

    wgpu::ShaderModuleWGSLDescriptor fsWGSLDesc = {};
    fsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    fsWGSLDesc.chain.next = nullptr;
    fsWGSLDesc.code = fragmentShaderSource;

    wgpu::ShaderModuleDescriptor fsDesc = {};
    fsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&fsWGSLDesc);
    fsDesc.label = "Fragment Shader";
    wgpu::ShaderModule fragmentShader = device.createShaderModule(fsDesc);
    
    if (!vertexShader || !fragmentShader) {
        std::cout << "ERROR: Failed to create shader modules!" << std::endl;
        return false;
    }
    
    // 顶点布局
    wgpu::VertexAttribute vertexAttributes[2] = {};
    vertexAttributes[0].offset = 0;
    vertexAttributes[0].shaderLocation = 0;
    vertexAttributes[0].format = wgpu::VertexFormat::Float32x2;
    vertexAttributes[1].offset = 2 * sizeof(float);
    vertexAttributes[1].shaderLocation = 1;
    vertexAttributes[1].format = wgpu::VertexFormat::Float32x2;
    
    wgpu::VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = 4 * sizeof(float);
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = vertexAttributes;
    
    // 创建渲染管线
    wgpu::RenderPipelineDescriptor renderPipelineDesc = {};
    renderPipelineDesc.label = "Render Pipeline";
    
    renderPipelineDesc.vertex.module = vertexShader;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.vertex.bufferCount = 1;
    renderPipelineDesc.vertex.buffers = &vertexBufferLayout;
    wgpu::StencilFaceState stencil{};
    stencil.compare      = wgpu::CompareFunction::Always;   // 不用模板测试
    stencil.failOp       = wgpu::StencilOperation::Keep;
    stencil.depthFailOp  = wgpu::StencilOperation::Keep;
    stencil.passOp       = wgpu::StencilOperation::Keep;
    wgpu::DepthStencilState ds{};
    ds.format            = wgpu::TextureFormat::Depth24Plus; // 必须和 Pass 一致
    ds.depthWriteEnabled = false;                            // 2D 不写深度
    ds.stencilFront      = stencil;
    ds.stencilBack       = stencil;         // ✨ 必填
    ds.stencilReadMask   = 0xFFFFFFFF;      // 合法掩码
    ds.stencilWriteMask  = 0xFFFFFFFF;      // 合法掩码
    ds.depthBias         = 0;
    ds.depthBiasSlopeScale = 0.0f;
    ds.depthBiasClamp    = 0.0f;
    ds.depthCompare      = wgpu::CompareFunction::Always;    // 总是通过
    renderPipelineDesc.depthStencil = &ds;

    
    wgpu::FragmentState fragmentState = {};
    fragmentState.module = fragmentShader;
    fragmentState.entryPoint = "main";
    
    // 创建Alpha混合状态
    wgpu::BlendState blendState = {};
    // RGB通道混合
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    // Alpha通道混合
    blendState.alpha.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::One;
    blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    wgpu::ColorTargetState colorTarget = {};
    colorTarget.format = swapChainFormat;  
    colorTarget.writeMask = wgpu::ColorWriteMask::All;
    colorTarget.blend = &blendState;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    renderPipelineDesc.fragment = &fragmentState;
    
    wgpu::MultisampleState multisampleState = {};
    multisampleState.count = 1;
    multisampleState.mask = 0xFFFFFFFF;
    multisampleState.alphaToCoverageEnabled = false;
    renderPipelineDesc.multisample = multisampleState;
    
    renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    renderPipelineDesc.primitive.cullMode = wgpu::CullMode::None;
    renderPipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    renderPipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    
    pipeline = device.createRenderPipeline(renderPipelineDesc);
    
    if (!pipeline) {
        std::cout << "ERROR: Failed to create render pipeline!" << std::endl;
        vertexShader.release();
        fragmentShader.release();
        return false;
    }
    
    vertexShader.release();
    fragmentShader.release();

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