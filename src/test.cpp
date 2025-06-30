// TransferFunctionTest.cpp
#include "test.h"
#include <iostream>
#include <cmath>
#include "GLFW/glfw3.h" 

TransferFunctionTest::TransferFunctionTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat)
    : m_device(device), m_queue(queue), m_swapChainFormat(swapChainFormat) {

        switch(m_swapChainFormat) {
        case wgpu::TextureFormat::BGRA8Unorm:
            std::cout << "Format: BGRA8Unorm" << std::endl;
            break;
        case wgpu::TextureFormat::BGRA8UnormSrgb:
            std::cout << "Format: BGRA8UnormSrgb" << std::endl;
            break;
        case wgpu::TextureFormat::RGBA8Unorm:
            std::cout << "Format: RGBA8Unorm" << std::endl;
            break;
        case wgpu::TextureFormat::RGBA8UnormSrgb:
            std::cout << "Format: RGBA8UnormSrgb" << std::endl;
            break;
        default:
            std::cout << "Format: Other" << std::endl;
            break;
    }
    std::cout << "====================================" << std::endl;

}

TransferFunctionTest::~TransferFunctionTest() {
    // 释放资源
    for (auto& texture : m_inputTFTextures) {
        if (texture) texture.release();
    }
    for (auto& view : m_inputTFViews) {
        if (view) view.release();
    }
    for (auto& bindGroup : m_computeBindGroups) {
        if (bindGroup) bindGroup.release();
    }
    
    if (m_outputTexture) m_outputTexture.release();
    if (m_outputTextureView) m_outputTextureView.release();
    if (m_vertexBuffer) m_vertexBuffer.release();
    if (m_renderSampler) m_renderSampler.release();
    if (m_renderBindGroup) m_renderBindGroup.release();
    if (m_computePipeline) m_computePipeline.release();
    if (m_renderPipeline) m_renderPipeline.release();
}

bool TransferFunctionTest::Initialize() {
    if (!CreateInputTFTextures()) return false;
    if (!CreateResources()) return false;
    if (!CreateComputePipeline()) return false;
    if (!CreateRenderPipeline()) return false;
    
    RunComputeShader(); // 初始运行一次
    
    return true;
}

bool TransferFunctionTest::CreateInputTFTextures() {
    // 创建3个不同的传输函数纹理
    struct TFMode {
        std::string name;
        std::vector<uint8_t> pixels; // RGBA8Unorm格式
    };
    
    std::vector<TFMode> tfModes(3);
    
    // 为每个模式生成256x1的RGBA数据 - 这部分保持不变
    for (int mode = 0; mode < 3; mode++) {
        tfModes[mode].pixels.resize(256 * 4); // 256 pixels * RGBA
        
        for (int i = 0; i < 256; i++) {
            float t = i / 255.0f;
            int idx = i * 4;
            
            switch (mode) {
                case 0: // 红色渐变
                    tfModes[mode].name = "Red Gradient";
                    tfModes[mode].pixels[idx + 0] = (uint8_t)(t * 255);  // R
                    tfModes[mode].pixels[idx + 1] = 0;                   // G  
                    tfModes[mode].pixels[idx + 2] = 0;                   // B
                    tfModes[mode].pixels[idx + 3] = 255;                 // A
                    break;
                    
                case 1: // 蓝色渐变
                    tfModes[mode].name = "Blue Gradient";
                    tfModes[mode].pixels[idx + 0] = 0;                   // R
                    tfModes[mode].pixels[idx + 1] = 0;                   // G
                    tfModes[mode].pixels[idx + 2] = (uint8_t)(t * 255);  // B  
                    tfModes[mode].pixels[idx + 3] = 255;                 // A
                    break;
                    
                case 2: // 彩虹色 (HSV to RGB)
                    {
                        tfModes[mode].name = "Rainbow";
                        float h = t * 360.0f;
                        float s = 1.0f;
                        float v = 1.0f;
                        
                        float c = v * s;
                        float x = c * (1 - abs(fmod(h / 60.0f, 2) - 1));
                        float m = v - c;
                        
                        float r, g, b;
                        if (h < 60) { r = c; g = x; b = 0; }
                        else if (h < 120) { r = x; g = c; b = 0; }
                        else if (h < 180) { r = 0; g = c; b = x; }
                        else if (h < 240) { r = 0; g = x; b = c; }
                        else if (h < 300) { r = x; g = 0; b = c; }
                        else { r = c; g = 0; b = x; }
                        
                        tfModes[mode].pixels[idx + 0] = (uint8_t)((r + m) * 255);
                        tfModes[mode].pixels[idx + 1] = (uint8_t)((g + m) * 255);
                        tfModes[mode].pixels[idx + 2] = (uint8_t)((b + m) * 255);
                        tfModes[mode].pixels[idx + 3] = 255;
                    }
                    break;
            }
        }
        
        std::cout << "Created TF mode: " << tfModes[mode].name << std::endl;
    }
    
    // 为每个模式创建纹理
    for (int i = 0; i < 3; i++) {
        std::cout << "Creating texture " << i << "..." << std::endl;
        
        // 创建纹理 - 添加缺少的字段
        wgpu::TextureDescriptor tfTextureDesc = {};
        tfTextureDesc.label = ("Input TF Texture " + std::to_string(i)).c_str();
        tfTextureDesc.dimension = wgpu::TextureDimension::_2D;
        tfTextureDesc.size = {256, 1, 1};
        tfTextureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        tfTextureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        tfTextureDesc.mipLevelCount = 1;        // 添加这个！
        tfTextureDesc.sampleCount = 1;          // 添加这个！
        tfTextureDesc.viewFormatCount = 0;      // 添加这个！
        tfTextureDesc.viewFormats = nullptr;    // 添加这个！
        
        wgpu::Texture texture = m_device.createTexture(tfTextureDesc);
        if (!texture) {
            std::cout << "ERROR: Failed to create texture " << i << std::endl;
            return false;
        }
        std::cout << "Texture " << i << " created successfully" << std::endl;
        
        m_inputTFTextures.push_back(texture);
        
        // 创建TextureView - 添加完整的描述符
        wgpu::TextureViewDescriptor viewDesc = {};
        viewDesc.label = ("Input TF View " + std::to_string(i)).c_str();
        viewDesc.format = wgpu::TextureFormat::RGBA8Unorm;      // 添加格式
        viewDesc.dimension = wgpu::TextureViewDimension::_2D;   // 添加维度
        viewDesc.baseMipLevel = 0;                              // 添加mip level
        viewDesc.mipLevelCount = 1;                             // 添加mip count
        viewDesc.baseArrayLayer = 0;                            // 添加array layer
        viewDesc.arrayLayerCount = 1;                           // 添加array count
        viewDesc.aspect = wgpu::TextureAspect::All;             // 添加aspect
        
        wgpu::TextureView view = texture.createView(viewDesc);
        if (!view) {
            std::cout << "ERROR: Failed to create texture view " << i << std::endl;
            return false;
        }
        std::cout << "Texture view " << i << " created successfully" << std::endl;
        
        m_inputTFViews.push_back(view);
        
        // 上传数据到纹理
        std::cout << "Uploading data to texture " << i << "..." << std::endl;
        wgpu::ImageCopyTexture destination = {};
        destination.texture = texture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = wgpu::TextureAspect::All;
        
        wgpu::TextureDataLayout dataLayout = {};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = 256 * 4; // 256 pixels * 4 bytes per pixel
        dataLayout.rowsPerImage = 1;
        
        wgpu::Extent3D extent = {256, 1, 1};
        
        m_queue.writeTexture(destination, tfModes[i].pixels.data(), 
                           tfModes[i].pixels.size(), dataLayout, extent);
                           
        std::cout << "Data uploaded to texture " << i << " successfully" << std::endl;
    }
    
    std::cout << "All input TF textures created successfully!" << std::endl;
    return true;
}

// 同时修复 CreateResources() 中的输出纹理创建
bool TransferFunctionTest::CreateResources() {
    std::cout << "Creating resources..." << std::endl;
    
    
    // 2. 创建输出纹理（CS的结果） - 修复所有缺少的字段
    wgpu::TextureDescriptor outputTextureDesc = {};
    outputTextureDesc.label = "CS Output Texture";
    outputTextureDesc.dimension = wgpu::TextureDimension::_2D;
    outputTextureDesc.size = {512, 512, 1}; // 输出一个512x512的纹理
    outputTextureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    outputTextureDesc.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
    outputTextureDesc.mipLevelCount = 1;        // 必需！
    outputTextureDesc.sampleCount = 1;          // 必需！修复 "Sample count 0 is invalid"
    outputTextureDesc.viewFormatCount = 0;      // 必需！
    outputTextureDesc.viewFormats = nullptr;    // 必需！
    
    m_outputTexture = m_device.createTexture(outputTextureDesc);
    if (!m_outputTexture) {
        std::cout << "ERROR: Failed to create output texture" << std::endl;
        return false;
    }
    std::cout << "Output texture created successfully" << std::endl;
    
    wgpu::TextureViewDescriptor outputViewDesc = {};
    outputViewDesc.label = "CS Output View";
    outputViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;      // 必需！
    outputViewDesc.dimension = wgpu::TextureViewDimension::_2D;   // 必需！
    outputViewDesc.baseMipLevel = 0;                              // 必需！
    outputViewDesc.mipLevelCount = 1;                             // 必需！修复 "invalid mipLevelCount"
    outputViewDesc.baseArrayLayer = 0;                            // 必需！
    outputViewDesc.arrayLayerCount = 1;                           // 必需！
    outputViewDesc.aspect = wgpu::TextureAspect::All;             // 必需！
    
    m_outputTextureView = m_outputTexture.createView(outputViewDesc);
    if (!m_outputTextureView) {
        std::cout << "ERROR: Failed to create output texture view" << std::endl;
        return false;
    }
    std::cout << "Output texture view created successfully" << std::endl;
    
    // 3. 创建渲染采样器
    wgpu::SamplerDescriptor renderSamplerDesc = {};
    renderSamplerDesc.label = "Render Sampler";
    renderSamplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    renderSamplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    renderSamplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;  // 添加W轴
    renderSamplerDesc.magFilter = wgpu::FilterMode::Linear;
    renderSamplerDesc.minFilter = wgpu::FilterMode::Linear;
    renderSamplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;  // 添加mipmap过滤
    renderSamplerDesc.lodMinClamp = 0.0f;                             // 添加LOD范围
    renderSamplerDesc.lodMaxClamp = 1.0f;
    renderSamplerDesc.maxAnisotropy = 1;                              // 修复：设置为1而不是0
    m_renderSampler = m_device.createSampler(renderSamplerDesc);
    
    if (!m_renderSampler) {
        std::cout << "ERROR: Failed to create render sampler" << std::endl;
        return false;
    }
    std::cout << "Render sampler created successfully" << std::endl;
    
    // 4. 创建全屏四边形顶点
    float vertices[] = {
        // 位置(x,y)    纹理坐标(u,v) 
        -1.0f, -1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 0.0f,
    };
    
    wgpu::BufferDescriptor vertexBufferDesc = {};
    vertexBufferDesc.label = "Vertex Buffer";
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = wgpu::BufferUsage::Vertex;
    vertexBufferDesc.mappedAtCreation = true;
    m_vertexBuffer = m_device.createBuffer(vertexBufferDesc);
    
    if (!m_vertexBuffer) {
        std::cout << "ERROR: Failed to create vertex buffer" << std::endl;
        return false;
    }
    
    void* vertexData = m_vertexBuffer.getMappedRange(0, sizeof(vertices));
    memcpy(vertexData, vertices, sizeof(vertices));
    m_vertexBuffer.unmap();
    
    std::cout << "Vertex buffer created successfully" << std::endl;
    std::cout << "All resources created successfully!" << std::endl;
    
    return true;
}


bool TransferFunctionTest::CreateComputePipeline() {
    // Compute Shader: 注意不能使用 textureSample！
    const char* computeShaderSource = R"(
        @group(0) @binding(0) var inputTF: texture_2d<f32>;
        @group(0) @binding(1) var outputTexture: texture_storage_2d<rgba8unorm, write>;
        
        @compute @workgroup_size(16, 16)
        fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
            let texSize = textureDimensions(outputTexture);
            let coord = vec2<i32>(i32(global_id.x), i32(global_id.y));
            
            if (coord.x >= i32(texSize.x) || coord.y >= i32(texSize.y)) {
                return;
            }
            
            // 将像素坐标归一化到[0,1]
            let uv = vec2<f32>(f32(coord.x) / f32(texSize.x), f32(coord.y) / f32(texSize.y));
            
            // 简单的水平颜色映射：X坐标直接对应传输函数
            let tfX = uv.x;
            
            // 计算传输函数纹理中的像素坐标
            let tfTexSize = textureDimensions(inputTF);
            let tfPixelX = clamp(i32(tfX * f32(tfTexSize.x)), 0, i32(tfTexSize.x) - 1);
            let tfPixelCoord = vec2<i32>(tfPixelX, 0);
            
            // 直接读取传输函数颜色，无任何修改
            let color = textureLoad(inputTF, tfPixelCoord, 0);
            
            // 直接输出，不做任何处理
            textureStore(outputTexture, coord, color);
        }
    )";
    
    std::cout << "Creating compute shader..." << std::endl;
    
    // 创建计算着色器模块
    wgpu::ShaderModuleWGSLDescriptor csWGSLDesc = {};
    csWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    csWGSLDesc.chain.next = nullptr;
    csWGSLDesc.code = computeShaderSource;

    wgpu::ShaderModuleDescriptor csDesc = {};
    csDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&csWGSLDesc);
    csDesc.label = "Compute Shader";
    wgpu::ShaderModule computeShader = m_device.createShaderModule(csDesc);

    if (!computeShader) {
        std::cout << "ERROR: Failed to create compute shader module!" << std::endl;
        return false;
    }
    std::cout << "Compute shader created successfully" << std::endl;
    
    // 创建计算管线
    std::cout << "Creating compute pipeline..." << std::endl;
    wgpu::ComputePipelineDescriptor computePipelineDesc = {};
    computePipelineDesc.label = "Compute Pipeline";
    computePipelineDesc.compute.module = computeShader;
    computePipelineDesc.compute.entryPoint = "main";
    m_computePipeline = m_device.createComputePipeline(computePipelineDesc);
    
    if (!m_computePipeline) {
        std::cout << "ERROR: Failed to create compute pipeline!" << std::endl;
        computeShader.release();
        return false;
    }
    std::cout << "Compute pipeline created successfully" << std::endl;
    
    // 为每个TF创建绑定组
    std::cout << "Creating compute bind groups..." << std::endl;
    for (int i = 0; i < 3; i++) {
        wgpu::BindGroupEntry computeBindGroupEntries[2] = {};
        computeBindGroupEntries[0].binding = 0;
        computeBindGroupEntries[0].textureView = m_inputTFViews[i];
        
        computeBindGroupEntries[1].binding = 1;
        computeBindGroupEntries[1].textureView = m_outputTextureView;
        
        wgpu::BindGroupDescriptor computeBindGroupDesc = {};
        computeBindGroupDesc.label = ("Compute Bind Group " + std::to_string(i)).c_str();
        computeBindGroupDesc.layout = m_computePipeline.getBindGroupLayout(0);
        computeBindGroupDesc.entryCount = 2;
        computeBindGroupDesc.entries = computeBindGroupEntries;
        
        wgpu::BindGroup bindGroup = m_device.createBindGroup(computeBindGroupDesc);
        if (!bindGroup) {
            std::cout << "ERROR: Failed to create compute bind group " << i << std::endl;
            computeShader.release();
            return false;
        }
        m_computeBindGroups.push_back(bindGroup);
        std::cout << "Compute bind group " << i << " created successfully" << std::endl;
    }
    
    computeShader.release();
    std::cout << "All compute resources created successfully!" << std::endl;
    return true;
}

// 修复 CreateRenderPipeline() 方法
bool TransferFunctionTest::CreateRenderPipeline() {
    std::cout << "Creating render pipeline..." << std::endl;
    
    // Vertex Shader
    const char* vertexShaderSource = R"(
        struct VertexInput {
            @location(0) position: vec2<f32>,
            @location(1) texCoord: vec2<f32>,
        }
        
        struct VertexOutput {
            @builtin(position) position: vec4<f32>,
            @location(0) texCoord: vec2<f32>,
        }
        
        @vertex
        fn main(input: VertexInput) -> VertexOutput {
            var output: VertexOutput;
            output.position = vec4<f32>(input.position, 0.0, 1.0);
            output.texCoord = input.texCoord;
            return output;
        }
    )";
    
    // Fragment Shader
    const char* fragmentShaderSource = R"(
        @group(0) @binding(0) var resultTexture: texture_2d<f32>;
        @group(0) @binding(1) var resultSampler: sampler;
        
        @fragment
        fn main(@location(0) texCoord: vec2<f32>) -> @location(0) vec4<f32> {
            return textureSample(resultTexture, resultSampler, texCoord);
        }
    )";
    
    // 创建着色器模块
    std::cout << "Creating vertex shader..." << std::endl;
    wgpu::ShaderModuleWGSLDescriptor vsWGSLDesc = {};
    vsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    vsWGSLDesc.chain.next = nullptr;
    vsWGSLDesc.code = vertexShaderSource;

    wgpu::ShaderModuleDescriptor vsDesc = {};
    vsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&vsWGSLDesc);
    vsDesc.label = "Vertex Shader";
    wgpu::ShaderModule vertexShader = m_device.createShaderModule(vsDesc);

    if (!vertexShader) {
        std::cout << "ERROR: Failed to create vertex shader!" << std::endl;
        return false;
    }
    std::cout << "Vertex shader created successfully" << std::endl;

    // 创建片段着色器模块    
    std::cout << "Creating fragment shader..." << std::endl;
    wgpu::ShaderModuleWGSLDescriptor fsWGSLDesc = {};
    fsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    fsWGSLDesc.chain.next = nullptr;
    fsWGSLDesc.code = fragmentShaderSource;

    wgpu::ShaderModuleDescriptor fsDesc = {};
    fsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&fsWGSLDesc);
    fsDesc.label = "Fragment Shader";
    wgpu::ShaderModule fragmentShader = m_device.createShaderModule(fsDesc);
    
    if (!fragmentShader) {
        std::cout << "ERROR: Failed to create fragment shader!" << std::endl;
        vertexShader.release();
        return false;
    }
    std::cout << "Fragment shader created successfully" << std::endl;
    
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
    std::cout << "Creating render pipeline descriptor..." << std::endl;
    wgpu::RenderPipelineDescriptor renderPipelineDesc = {};
    renderPipelineDesc.label = "Render Pipeline";
    
    // 顶点阶段
    renderPipelineDesc.vertex.module = vertexShader;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.vertex.bufferCount = 1;
    renderPipelineDesc.vertex.buffers = &vertexBufferLayout;
    
    // 片段阶段
    wgpu::FragmentState fragmentState = {};
    fragmentState.module = fragmentShader;
    fragmentState.entryPoint = "main";
    
    wgpu::ColorTargetState colorTarget = {};
    colorTarget.format = m_swapChainFormat; // 你需要根据实际情况调整这个格式
    colorTarget.writeMask = wgpu::ColorWriteMask::All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    renderPipelineDesc.fragment = &fragmentState;
    
    // 重要：设置多重采样状态
    wgpu::MultisampleState multisampleState = {};
    multisampleState.count = 1;                    // 修复：明确设置采样数为1
    multisampleState.mask = 0xFFFFFFFF;           // 设置采样掩码
    multisampleState.alphaToCoverageEnabled = false;  // 禁用alpha to coverage
    renderPipelineDesc.multisample = multisampleState;
    
    // 其他设置
    renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    renderPipelineDesc.primitive.cullMode = wgpu::CullMode::None;
    renderPipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;  // 添加面朝向
    renderPipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined; // triangle strip不需要索引
    
    std::cout << "Creating render pipeline..." << std::endl;
    m_renderPipeline = m_device.createRenderPipeline(renderPipelineDesc);
    
    if (!m_renderPipeline) {
        std::cout << "ERROR: Failed to create render pipeline!" << std::endl;
        vertexShader.release();
        fragmentShader.release();
        return false;
    }
    std::cout << "Render pipeline created successfully" << std::endl;
    
    // 创建渲染绑定组
    std::cout << "Creating render bind group..." << std::endl;
    wgpu::BindGroupEntry renderBindGroupEntries[2] = {};
    renderBindGroupEntries[0].binding = 0;
    renderBindGroupEntries[0].textureView = m_outputTextureView;
    renderBindGroupEntries[1].binding = 1;
    renderBindGroupEntries[1].sampler = m_renderSampler;
    
    wgpu::BindGroupDescriptor renderBindGroupDesc = {};
    renderBindGroupDesc.label = "Render Bind Group";
    renderBindGroupDesc.layout = m_renderPipeline.getBindGroupLayout(0);
    renderBindGroupDesc.entryCount = 2;
    renderBindGroupDesc.entries = renderBindGroupEntries;
    m_renderBindGroup = m_device.createBindGroup(renderBindGroupDesc);
    
    if (!m_renderBindGroup) {
        std::cout << "ERROR: Failed to create render bind group!" << std::endl;
        vertexShader.release();
        fragmentShader.release();
        return false;
    }
    std::cout << "Render bind group created successfully" << std::endl;
    
    vertexShader.release();
    fragmentShader.release();
    
    std::cout << "Render pipeline creation completed successfully!" << std::endl;
    return true;
}

void TransferFunctionTest::SwitchTransferFunction() {
    if (m_useExternalTF && m_externalTFView) {
        std::cout << "Using external UI Transfer Function" << std::endl;
    } else {
        std::cout << "Using built-in TF index: " << m_currentTFIndex << std::endl;
    }
    RunComputeShader();
}
void TransferFunctionTest::SetExternalTransferFunction(wgpu::TextureView tfTextureView)
{
    m_externalTFView = tfTextureView;
    if (m_externalTFView) {
        UpdateExternalBindGroup();
        std::cout << "External Transfer Function set and bind group updated" << std::endl;
    }
}

void TransferFunctionTest::UpdateExternalBindGroup()
{
    if (!m_externalTFView || !m_computePipeline) {
        return;
    }
    
    // 释放旧的绑定组
    if (m_externalBindGroup) {
        m_externalBindGroup.release();
        m_externalBindGroup = nullptr;
    }
    
    // 创建新的绑定组
    wgpu::BindGroupEntry entries[2] = {};
    entries[0].binding = 0;
    entries[0].textureView = m_externalTFView;
    
    entries[1].binding = 1;
    entries[1].textureView = m_outputTextureView;
    
    wgpu::BindGroupDescriptor desc = {};
    desc.label = "External TF Bind Group";
    desc.layout = m_computePipeline.getBindGroupLayout(0);
    desc.entryCount = 2;
    desc.entries = entries;
    
    m_externalBindGroup = m_device.createBindGroup(desc);
    
    std::cout << "External bind group created successfully" << std::endl;
}

void TransferFunctionTest::RunComputeShader() {
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "Compute Command Encoder";
    wgpu::CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);
    
    wgpu::ComputePassDescriptor computePassDesc = {};
    computePassDesc.label = "Compute Pass";
    wgpu::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
    
    computePass.setPipeline(m_computePipeline);
    
    // 根据当前模式选择合适的绑定组
    if (m_useExternalTF && m_externalBindGroup) {
        computePass.setBindGroup(0, m_externalBindGroup, 0, nullptr);
        std::cout << "Using external bind group for compute shader" << std::endl;
    } else if (m_currentTFIndex < m_computeBindGroups.size()) {
        computePass.setBindGroup(0, m_computeBindGroups[m_currentTFIndex], 0, nullptr);
        std::cout << "Using built-in bind group " << m_currentTFIndex << " for compute shader" << std::endl;
    } else {
        std::cout << "ERROR: No valid bind group available!" << std::endl;
        computePass.end();
        computePass.release();
        encoder.release();
        return;
    }
    
    computePass.dispatchWorkgroups((512 + 15) / 16, (512 + 15) / 16, 1);
    
    computePass.end();
    computePass.release();
    
    wgpu::CommandBufferDescriptor cmdBufferDesc = {};
    cmdBufferDesc.label = "Compute Command Buffer";
    wgpu::CommandBuffer commandBuffer = encoder.finish(cmdBufferDesc);
    encoder.release();
    
    m_queue.submit(1, &commandBuffer);
    commandBuffer.release();
}

void TransferFunctionTest::Render(wgpu::RenderPassEncoder renderPass) {
    renderPass.setPipeline(m_renderPipeline);
    renderPass.setBindGroup(0, m_renderBindGroup, 0, nullptr);
    renderPass.setVertexBuffer(0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
    renderPass.draw(4, 1, 0, 0); // 绘制四边形 (triangle strip)
}

void TransferFunctionTest::OnKeyPress(int key, int action) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_J) {
            if (m_useExternalTF) {
                // 从外部TF切换回内置TF
                m_useExternalTF = false;
                m_currentTFIndex = 0;  // 切换到第一个内置TF
                std::cout << "Switched to built-in TF: " << m_currentTFIndex << std::endl;
            } else {
                // 在内置TF之间切换
                m_currentTFIndex = (m_currentTFIndex - 1 + 3) % 3;
                std::cout << "Switched to built-in TF: " << m_currentTFIndex << std::endl;
            }
            SwitchTransferFunction();
        }
        else if (key == GLFW_KEY_K) {
            if (m_useExternalTF) {
                // 从外部TF切换回内置TF
                m_useExternalTF = false;
                m_currentTFIndex = 0;  // 切换到第一个内置TF
                std::cout << "Switched to built-in TF: " << m_currentTFIndex << std::endl;
            } else {
                // 在内置TF之间切换，或者切换到外部TF
                if (m_currentTFIndex == 2 && m_externalTFView) {
                    // 如果已经是最后一个内置TF且有外部TF，切换到外部TF
                    m_useExternalTF = true;
                    std::cout << "Switched to external UI TF" << std::endl;
                } else {
                    // 否则继续在内置TF之间切换
                    m_currentTFIndex = (m_currentTFIndex + 1) % 3;
                    std::cout << "Switched to built-in TF: " << m_currentTFIndex << std::endl;
                }
            }
            SwitchTransferFunction();
        }
    }
}

void TransferFunctionTest::OnWindowResize(int width, int height) {
    std::cout << "TransferFunctionTest: Window resized to " << width << "x" << height << std::endl;
}