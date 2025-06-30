// TransferFunctionTest.cpp
#include "test.h"
#include <iostream>
#include <cmath>
#include "GLFW/glfw3.h" 

TransferFunctionTest::TransferFunctionTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat)
    : m_device(device), m_queue(queue), m_swapChainFormat(swapChainFormat) {
    
    std::cout << "=== TransferFunctionTest Created ===" << std::endl;
    std::cout << "SwapChain Format: " << static_cast<int>(m_swapChainFormat) << std::endl;
}

TransferFunctionTest::~TransferFunctionTest() {
    // 释放资源
    if (m_computeBindGroup) m_computeBindGroup.release();
    if (m_outputTexture) m_outputTexture.release();
    if (m_outputTextureView) m_outputTextureView.release();
    if (m_vertexBuffer) m_vertexBuffer.release();
    if (m_renderSampler) m_renderSampler.release();
    if (m_renderBindGroup) m_renderBindGroup.release();
    if (m_computePipeline) m_computePipeline.release();
    if (m_renderPipeline) m_renderPipeline.release();
}

bool TransferFunctionTest::Initialize() {
    std::cout << "Initializing Transfer Function Test..." << std::endl;
    
    if (!CreateResources()) return false;
    if (!CreateComputePipeline()) return false;
    if (!CreateRenderPipeline()) return false;
    
    std::cout << "Transfer Function Test initialized successfully!" << std::endl;
    return true;
}

void TransferFunctionTest::SetExternalTransferFunction(wgpu::TextureView tfTextureView)
{
    m_tfTextureView = tfTextureView;
    if (m_tfTextureView) {
        UpdateComputeBindGroup();
        m_needsUpdate = true;  // 标记需要更新，不立即运行CS      
    }
}
void TransferFunctionTest::UpdateComputeBindGroup()
{
    if (!m_tfTextureView || !m_computePipeline) {
        return;
    }
    
    // 释放旧的绑定组
    if (m_computeBindGroup) {
        m_computeBindGroup.release();
        m_computeBindGroup = nullptr;
    }
    
    // 创建新的绑定组
    wgpu::BindGroupEntry entries[2] = {};
    entries[0].binding = 0;
    entries[0].textureView = m_tfTextureView;
    
    entries[1].binding = 1;
    entries[1].textureView = m_outputTextureView;
    
    wgpu::BindGroupDescriptor desc = {};
    desc.label = "Compute Bind Group";
    desc.layout = m_computePipeline.getBindGroupLayout(0);
    desc.entryCount = 2;
    desc.entries = entries;
    
    m_computeBindGroup = m_device.createBindGroup(desc);
    
    if (m_computeBindGroup) {
        // std::cout << "Compute bind group updated successfully" << std::endl;
    } else {
        std::cout << "ERROR: Failed to create compute bind group" << std::endl;
    }
}



bool TransferFunctionTest::CreateResources() {
    std::cout << "Creating resources..." << std::endl;
    
    // 1. 创建输出纹理（CS的结果）
    wgpu::TextureDescriptor outputTextureDesc = {};
    outputTextureDesc.label = "CS Output Texture";
    outputTextureDesc.dimension = wgpu::TextureDimension::_2D;
    outputTextureDesc.size = {512, 512, 1};
    outputTextureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    outputTextureDesc.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
    outputTextureDesc.mipLevelCount = 1;
    outputTextureDesc.sampleCount = 1;
    outputTextureDesc.viewFormatCount = 0;
    outputTextureDesc.viewFormats = nullptr;
    
    m_outputTexture = m_device.createTexture(outputTextureDesc);
    if (!m_outputTexture) {
        std::cout << "ERROR: Failed to create output texture" << std::endl;
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
        std::cout << "ERROR: Failed to create output texture view" << std::endl;
        return false;
    }
    
    // 2. 创建渲染采样器
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
    m_renderSampler = m_device.createSampler(renderSamplerDesc);
    
    if (!m_renderSampler) {
        std::cout << "ERROR: Failed to create render sampler" << std::endl;
        return false;
    }
    
    // 3. 创建全屏四边形顶点
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
    
    std::cout << "All resources created successfully!" << std::endl;
    return true;
}

bool TransferFunctionTest::CreateComputePipeline() {
    // 简单的颜色映射计算着色器
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
    
    std::cout << "Creating compute shader..." << std::endl;
    
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
    
    // 创建计算管线
    wgpu::ComputePipelineDescriptor computePipelineDesc = {};
    computePipelineDesc.label = "Compute Pipeline";
    computePipelineDesc.compute.module = computeShader;
    computePipelineDesc.compute.entryPoint = "main";
    m_computePipeline = m_device.createComputePipeline(computePipelineDesc);
    
    computeShader.release();
    
    if (!m_computePipeline) {
        std::cout << "ERROR: Failed to create compute pipeline!" << std::endl;
        return false;
    }
    
    std::cout << "Compute pipeline created successfully" << std::endl;
    return true;
}

bool TransferFunctionTest::CreateRenderPipeline() {
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
            let color = textureSample(resultTexture, resultSampler, texCoord);
            // return vec4<f32>(color.a, color.a, color.a, 1.0);
            return color;  // 返回采样的颜色
        }
    )";
    
    std::cout << "Creating render pipeline..." << std::endl;
    
    // 创建着色器模块
    wgpu::ShaderModuleWGSLDescriptor vsWGSLDesc = {};
    vsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    vsWGSLDesc.chain.next = nullptr;
    vsWGSLDesc.code = vertexShaderSource;

    wgpu::ShaderModuleDescriptor vsDesc = {};
    vsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&vsWGSLDesc);
    vsDesc.label = "Vertex Shader";
    wgpu::ShaderModule vertexShader = m_device.createShaderModule(vsDesc);

    wgpu::ShaderModuleWGSLDescriptor fsWGSLDesc = {};
    fsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    fsWGSLDesc.chain.next = nullptr;
    fsWGSLDesc.code = fragmentShaderSource;

    wgpu::ShaderModuleDescriptor fsDesc = {};
    fsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&fsWGSLDesc);
    fsDesc.label = "Fragment Shader";
    wgpu::ShaderModule fragmentShader = m_device.createShaderModule(fsDesc);
    
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
    colorTarget.format = m_swapChainFormat;  // 使用正确的格式
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
    
    m_renderPipeline = m_device.createRenderPipeline(renderPipelineDesc);
    
    if (!m_renderPipeline) {
        std::cout << "ERROR: Failed to create render pipeline!" << std::endl;
        vertexShader.release();
        fragmentShader.release();
        return false;
    }
    
    // 创建渲染绑定组
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
    
    vertexShader.release();
    fragmentShader.release();
    
    if (!m_renderBindGroup) {
        std::cout << "ERROR: Failed to create render bind group!" << std::endl;
        return false;
    }
    
    std::cout << "Render pipeline created successfully!" << std::endl;
    return true;
}

void TransferFunctionTest::RunComputeShader() {
    if (!m_computeBindGroup) {
        std::cout << "No compute bind group available, skipping compute shader" << std::endl;
        return;
    }
    
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "Compute Command Encoder";
    wgpu::CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);
    
    wgpu::ComputePassDescriptor computePassDesc = {};
    computePassDesc.label = "Compute Pass";
    wgpu::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
    
    computePass.setPipeline(m_computePipeline);
    computePass.setBindGroup(0, m_computeBindGroup, 0, nullptr);
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

bool TransferFunctionTest::CheckAndPrepareUpdate(wgpu::TextureView tfTextureView) {
    // 检查是否需要更新
    if (m_lastTFView != tfTextureView) {
        m_lastTFView = tfTextureView;
        m_tfTextureView = tfTextureView;
        
        if (m_tfTextureView) {
            UpdateComputeBindGroup();
            return true;  // 需要运行计算着色器
        }
    }
    return false;  // 不需要更新
}

void TransferFunctionTest::RenderWithTF(wgpu::RenderPassEncoder renderPass, wgpu::TextureView tfTextureView) {
    // 只是标记需要更新，不在这里运行计算着色器
    if (m_tfTextureView != tfTextureView) {
        std::cout << "TF changed, marking for update..." << std::endl;
        m_tfTextureView = tfTextureView;
        UpdateComputeBindGroup();
        m_needsUpdate = true;  // 标记需要在下一帧更新
    }
    
    // 渲染当前状态
    Render(renderPass);
}


void TransferFunctionTest::UpdateIfNeeded() {
    if (m_needsUpdate && m_computeBindGroup) {
        m_needsUpdate = false;
        RunComputeShader();
        
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

void TransferFunctionTest::Render(wgpu::RenderPassEncoder renderPass) {
    
    
    if (!m_renderPipeline || !m_renderBindGroup || !m_vertexBuffer) {
        return;
    }

    
    
    renderPass.setPipeline(m_renderPipeline);
    renderPass.setBindGroup(0, m_renderBindGroup, 0, nullptr);
    renderPass.setVertexBuffer(0, m_vertexBuffer, 0, WGPU_WHOLE_SIZE);
    renderPass.draw(4, 1, 0, 0);
}

void TransferFunctionTest::OnKeyPress(int key, int action) {
    // 保留这个方法以维持接口一致性，但现在不需要按键处理
    // 如果将来需要其他按键功能，可以在这里添加
}

void TransferFunctionTest::OnWindowResize(int width, int height) {
    std::cout << "TransferFunctionTest: Window resized to " << width << "x" << height << std::endl;
}