#include "VolumeRenderingTest.h"

VolumeRenderingTest::VolumeRenderingTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat)
    : m_device(device), m_queue(queue), m_swapChainFormat(swapChainFormat) {
    std::cout << "[VolumeRenderingTest] === VolumeRenderingTest Created ===" << std::endl;
}

VolumeRenderingTest::~VolumeRenderingTest() {
    m_computeStage.Release();
    m_raycastingStage.Release();
    if (m_volumeTextureView) {
        m_volumeTextureView.release();
        m_volumeTextureView = nullptr;
    }
    if (m_volumeTexture) {
        m_volumeTexture.release();
        m_volumeTexture = nullptr;
    }
}

bool VolumeRenderingTest::Initialize(glm::mat4 vMat, glm::mat4 pMat) {
    std::cout << "[VolumeRenderingTest] Initializing Volume Rendering Test..." << std::endl;

    m_RS_Uniforms.viewMatrix = vMat;
    m_RS_Uniforms.projMatrix = pMat;
    
    // 设置相机位置（相对于体数据的位置）
    m_RS_Uniforms.cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    m_RS_Uniforms.rayStepSize = 0.01f;
    m_RS_Uniforms.volumeOpacity = 1.0f;
    m_RS_Uniforms.volumeSize = glm::vec3(1.0f);

    if (!InitVolumeTexture()) return false;
    if (!m_computeStage.Init(m_device, m_queue, m_CS_Uniforms)) return false;
    if (!m_raycastingStage.Init(m_device, m_queue, m_RS_Uniforms)) return false;
    if (!m_raycastingStage.CreatePipeline(m_device, m_swapChainFormat)) return false;
    
    std::cout << "[VolumeRenderingTest] Volume Rendering Test initialized successfully!" << std::endl;
    return true;
}

bool VolumeRenderingTest::InitVolumeTexture(uint32_t width, uint32_t height, uint32_t depth) {
    // 创建3D体纹理
    wgpu::TextureDescriptor volumeTextureDesc = {};
    volumeTextureDesc.label = "Volume Texture 3D";
    volumeTextureDesc.dimension = wgpu::TextureDimension::_3D;
    volumeTextureDesc.size = {width, height, depth};
    volumeTextureDesc.format = wgpu::TextureFormat::RGBA16Float;
    volumeTextureDesc.usage = wgpu::TextureUsage::StorageBinding |     // 计算着色器写入
                              wgpu::TextureUsage::TextureBinding;      // 渲染着色器读取
    volumeTextureDesc.mipLevelCount = 1;
    volumeTextureDesc.sampleCount = 1;
    volumeTextureDesc.viewFormatCount = 0;
    volumeTextureDesc.viewFormats = nullptr;

    m_volumeTexture = m_device.createTexture(volumeTextureDesc);
    if (!m_volumeTexture) {
        std::cout << "[ERROR]::InitVolumeTexture: Failed to create volume texture" << std::endl;
        return false;
    }

    wgpu::TextureViewDescriptor volumeViewDesc = {};
    volumeViewDesc.label = "Volume Texture View";
    volumeViewDesc.format = wgpu::TextureFormat::RGBA16Float;
    volumeViewDesc.dimension = wgpu::TextureViewDimension::_3D;
    volumeViewDesc.baseMipLevel = 0;
    volumeViewDesc.mipLevelCount = 1;
    volumeViewDesc.baseArrayLayer = 0;
    volumeViewDesc.arrayLayerCount = 1;
    volumeViewDesc.aspect = wgpu::TextureAspect::All;

    m_volumeTextureView = m_volumeTexture.createView(volumeViewDesc);
    if (!m_volumeTextureView) {
        std::cout << "[ERROR]::InitVolumeTexture: Failed to create volume texture view" << std::endl;
        return false;
    }

    // 更新计算着色器Uniform
    m_CS_Uniforms.gridWidth = static_cast<float>(width);
    m_CS_Uniforms.gridHeight = static_cast<float>(height);
    m_CS_Uniforms.gridDepth = static_cast<float>(depth);

    return true;
}

void VolumeRenderingTest::UpdateTransferFunction(wgpu::TextureView tfTextureView) {
    m_tfTextureView = tfTextureView;
    if (m_tfTextureView) {
        m_computeStage.UpdateBindGroup(m_device, m_tfTextureView, m_volumeTextureView);
        m_raycastingStage.InitBindGroup(m_device, m_volumeTextureView, m_tfTextureView);
        m_needsUpdate = true;
    }

    if (m_needsUpdate && m_computeStage.bindGroup && m_computeStage.pipeline) {
        m_needsUpdate = false;
        m_computeStage.RunCompute(m_device, m_queue);
        
        // 同步
        #if defined(WEBGPU_BACKEND_DAWN)
        for (int i = 0; i < 10; ++i) {
            m_device.tick();
        }
        #elif defined(WEBGPU_BACKEND_WGPU)
        m_device.poll(true);
        #endif
    }
}

void VolumeRenderingTest::UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix, glm::vec3 cameraPos) {
    m_RS_Uniforms.viewMatrix = viewMatrix;
    m_RS_Uniforms.projMatrix = projMatrix;
    m_RS_Uniforms.cameraPos = cameraPos;
    
    m_raycastingStage.UpdateUniforms(m_queue, m_RS_Uniforms);
}

void VolumeRenderingTest::SetRayStepSize(float stepSize) {
    if (m_RS_Uniforms.rayStepSize != stepSize) {
        m_RS_Uniforms.rayStepSize = stepSize;
        m_raycastingStage.UpdateUniforms(m_queue, m_RS_Uniforms);
    }
}

void VolumeRenderingTest::SetVolumeOpacity(float opacity) {
    if (m_RS_Uniforms.volumeOpacity != opacity) {
        m_RS_Uniforms.volumeOpacity = opacity;
        m_raycastingStage.UpdateUniforms(m_queue, m_RS_Uniforms);
    }
}

void VolumeRenderingTest::Render(wgpu::RenderPassEncoder renderPass) {
    m_raycastingStage.Render(renderPass);
}

// ============== ComputeStage Implementation ==============

bool VolumeRenderingTest::ComputeStage::Init(wgpu::Device device, wgpu::Queue queue, const CS_Uniforms_3D& uniforms) {
    if (!InitUBO(device, uniforms)) return false;
    if (!CreatePipeline(device)) return false;
    return true;
}

bool VolumeRenderingTest::ComputeStage::InitUBO(wgpu::Device device, const CS_Uniforms_3D& uniforms) {
    wgpu::BufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.label = "Volume Compute Uniform Buffer";
    uniformBufferDesc.size = sizeof(CS_Uniforms_3D);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniformBufferDesc.mappedAtCreation = false;
    
    uniformBuffer = device.createBuffer(uniformBufferDesc);
    if (!uniformBuffer) {
        std::cout << "[ERROR]::ComputeStage::InitUBO Failed to create uniform buffer" << std::endl;
        return false;
    }
    
    device.getQueue().writeBuffer(uniformBuffer, 0, &uniforms, sizeof(CS_Uniforms_3D));
    return true;
}

bool VolumeRenderingTest::ComputeStage::CreatePipeline(wgpu::Device device) {
    // 创建绑定组布局
    wgpu::BindGroupLayoutEntry entries[3] = {};
    
    // Binding 0: 3D输出纹理
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Compute;
    entries[0].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
    entries[0].storageTexture.format = wgpu::TextureFormat::RGBA16Float;
    entries[0].storageTexture.viewDimension = wgpu::TextureViewDimension::_3D;
    
    // Binding 1: Uniform Buffer
    entries[1].binding = 1;
    entries[1].visibility = wgpu::ShaderStage::Compute;
    entries[1].buffer.type = wgpu::BufferBindingType::Uniform;
    
    // Binding 2: 传输函数纹理（2D）
    entries[2].binding = 2;
    entries[2].visibility = wgpu::ShaderStage::Compute;
    entries[2].texture.sampleType = wgpu::TextureSampleType::Float;
    entries[2].texture.viewDimension = wgpu::TextureViewDimension::_2D;
    
    wgpu::BindGroupLayoutDescriptor layoutDesc = {};
    layoutDesc.label = "Volume Compute Bind Group Layout";
    layoutDesc.entryCount = 3;
    layoutDesc.entries = entries;
    auto bindGroupLayout = device.createBindGroupLayout(layoutDesc);
    
    auto& mgr = PipelineManager::getInstance();
    pipeline = mgr.createComputePipeline()
        .setDevice(device)
        .setLabel("Volume Compute Pipeline")
        .setShader("../shaders/volume_simple.comp.wgsl", "main")
        .setExplicitLayout(true)
        .addBindGroupLayout(bindGroupLayout)
        .build();
    
    bindGroupLayout.release();

    if (!pipeline) {
        std::cout << "[ERROR] Failed to create volume compute pipeline!" << std::endl;
        return false;
    }
    
    std::cout << "[VolumeRenderingTest] Volume compute pipeline created successfully" << std::endl;
    return true;
}

bool VolumeRenderingTest::ComputeStage::UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputVolume3D) {
    if (!inputTF || !pipeline || !uniformBuffer || !outputVolume3D) return false;
    
    if (bindGroup) {
        bindGroup.release();
        bindGroup = nullptr;
    }
    
    wgpu::BindGroupEntry entries[3] = {};
    entries[0].binding = 0;
    entries[0].textureView = outputVolume3D;
    entries[1].binding = 1;
    entries[1].buffer = uniformBuffer;
    entries[1].offset = 0;
    entries[1].size = sizeof(CS_Uniforms_3D);
    entries[2].binding = 2;
    entries[2].textureView = inputTF;

    wgpu::BindGroupDescriptor desc = {};
    desc.label = "Volume Compute Bind Group";
    desc.layout = pipeline.getBindGroupLayout(0);
    desc.entryCount = 3;
    desc.entries = entries;
    
    bindGroup = device.createBindGroup(desc);
    return bindGroup != nullptr;
}

void VolumeRenderingTest::ComputeStage::RunCompute(wgpu::Device device, wgpu::Queue queue) {
    if (!bindGroup || !pipeline) return;

    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "Volume Compute Command Encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);
    
    wgpu::ComputePassDescriptor computePassDesc = {};
    computePassDesc.label = "Volume Compute Pass";
    wgpu::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
    
    computePass.setPipeline(pipeline);
    computePass.setBindGroup(0, bindGroup, 0, nullptr);
    // 3D工作组：8x8x8 的工作组，覆盖 128x128x128 的体数据
    computePass.dispatchWorkgroups((128 + 7) / 8, (128 + 7) / 8, (128 + 7) / 8);
    
    computePass.end();
    computePass.release();
    
    wgpu::CommandBufferDescriptor cmdBufferDesc = {};
    cmdBufferDesc.label = "Volume Compute Command Buffer";
    wgpu::CommandBuffer commandBuffer = encoder.finish(cmdBufferDesc);
    encoder.release();
    
    queue.submit(1, &commandBuffer);
    commandBuffer.release();
}

void VolumeRenderingTest::ComputeStage::Release() {
    if (pipeline) {
        pipeline.release();
        pipeline = nullptr;
    }
    if (bindGroup) {
        bindGroup.release();
        bindGroup = nullptr;
    }
    if (uniformBuffer) {
        uniformBuffer.release();
        uniformBuffer = nullptr;
    }
}

// ============== RaycastingStage Implementation ==============

bool VolumeRenderingTest::RaycastingStage::Init(wgpu::Device device, wgpu::Queue queue, const RS_Uniforms_3D& uniforms) {
    if (!InitCubeGeometry(device, queue)) return false;
    if (!InitSamplers(device)) return false;
    if (!InitUBO(device, uniforms)) return false;
    return true;
}

bool VolumeRenderingTest::RaycastingStage::InitCubeGeometry(wgpu::Device device, wgpu::Queue queue) {
    // 立方体顶点（-0.5 到 0.5，后面在shader中变换到0-1）
    float vertices[] = {
        // 前面 (z = 0.5)
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        // 后面 (z = -0.5)
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
    };
    
    uint16_t indices[] = {
        // 前面
        0, 1, 2, 2, 3, 0,
        // 右面
        1, 5, 6, 6, 2, 1,
        // 后面
        7, 6, 5, 5, 4, 7,
        // 左面
        4, 0, 3, 3, 7, 4,
        // 底面
        4, 5, 1, 1, 0, 4,
        // 顶面
        3, 2, 6, 6, 7, 3
    };
    
    // 创建顶点缓冲区
    wgpu::BufferDescriptor vertexBufferDesc = {};
    vertexBufferDesc.label = "Cube Vertex Buffer";
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    vertexBufferDesc.mappedAtCreation = false;
    cubeVertexBuffer = device.createBuffer(vertexBufferDesc);
    
    if (!cubeVertexBuffer) {
        std::cout << "[ERROR]::InitCubeGeometry Failed to create vertex buffer" << std::endl;
        return false;
    }
    
    queue.writeBuffer(cubeVertexBuffer, 0, vertices, sizeof(vertices));
    
    // 创建索引缓冲区
    wgpu::BufferDescriptor indexBufferDesc = {};
    indexBufferDesc.label = "Cube Index Buffer";
    indexBufferDesc.size = sizeof(indices);
    indexBufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
    indexBufferDesc.mappedAtCreation = false;
    cubeIndexBuffer = device.createBuffer(indexBufferDesc);
    
    if (!cubeIndexBuffer) {
        std::cout << "[ERROR]::InitCubeGeometry Failed to create index buffer" << std::endl;
        return false;
    }
    
    queue.writeBuffer(cubeIndexBuffer, 0, indices, sizeof(indices));
    
    return true;
}

bool VolumeRenderingTest::RaycastingStage::InitSamplers(wgpu::Device device) {
    // 体数据采样器（线性插值）
    wgpu::SamplerDescriptor volumeSamplerDesc = {};
    volumeSamplerDesc.label = "Volume Sampler";
    volumeSamplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    volumeSamplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    volumeSamplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    volumeSamplerDesc.magFilter = wgpu::FilterMode::Linear;
    volumeSamplerDesc.minFilter = wgpu::FilterMode::Linear;
    volumeSamplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    volumeSampler = device.createSampler(volumeSamplerDesc);
    
    // 传输函数采样器
    wgpu::SamplerDescriptor tfSamplerDesc = {};
    tfSamplerDesc.label = "Transfer Function Sampler";
    tfSamplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    tfSamplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    tfSamplerDesc.magFilter = wgpu::FilterMode::Linear;
    tfSamplerDesc.minFilter = wgpu::FilterMode::Linear;
    tfSampler = device.createSampler(tfSamplerDesc);
    
    return volumeSampler != nullptr && tfSampler != nullptr;
}

bool VolumeRenderingTest::RaycastingStage::InitUBO(wgpu::Device device, const RS_Uniforms_3D& uniforms) {
    wgpu::BufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.label = "Raycasting Uniform Buffer";
    uniformBufferDesc.size = sizeof(RS_Uniforms_3D);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniformBufferDesc.mappedAtCreation = false;
    uniformBuffer = device.createBuffer(uniformBufferDesc);
    
    if (!uniformBuffer) {
        std::cout << "[ERROR]::RaycastingStage::InitUBO Failed to create uniform buffer" << std::endl;
        return false;
    }
    
    UpdateUniforms(device.getQueue(), uniforms);
    return true;
}

void VolumeRenderingTest::RaycastingStage::UpdateUniforms(wgpu::Queue queue, const RS_Uniforms_3D& uniforms) {
    queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(RS_Uniforms_3D));
}

bool VolumeRenderingTest::RaycastingStage::CreatePipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat) {
    auto& mgr = PipelineManager::getInstance();
    
    pipeline = mgr.createRenderPipeline()
        .setDevice(device)
        .setLabel("Volume Raycasting Pipeline")
        .setVertexShader("../shaders/volume_raycasting.vert.wgsl", "main")
        .setFragmentShader("../shaders/volume_raycasting.frag.wgsl", "main")
        // .setVertexLayout(VertexLayoutBuilder::createPosition())  // 只需要位置
        .setSwapChainFormat(swapChainFormat)
        .setAlphaBlending()
        .setReadOnlyDepth()
        .build();

    if (!pipeline) {
        std::cout << "[ERROR] Failed to create volume raycasting pipeline!" << std::endl;
        return false;
    }

    return true;
}

bool VolumeRenderingTest::RaycastingStage::InitBindGroup(wgpu::Device device, wgpu::TextureView volumeTexture, wgpu::TextureView transferFunction) {
    if (!volumeTexture || !transferFunction || !pipeline || !uniformBuffer || !volumeSampler || !tfSampler) {
        std::cout << "[ERROR]::RaycastingStage::InitBindGroup: Missing prerequisites" << std::endl;
        return false;
    }

    if (bindGroup) {
        bindGroup.release();
        bindGroup = nullptr;
    }

    wgpu::BindGroupEntry entries[5] = {};
    entries[0].binding = 0;
    entries[0].buffer = uniformBuffer;
    entries[0].offset = 0;
    entries[0].size = sizeof(RS_Uniforms_3D);
    entries[1].binding = 1;
    entries[1].textureView = volumeTexture;
    entries[2].binding = 2;
    entries[2].sampler = volumeSampler;
    entries[3].binding = 3;
    entries[3].textureView = transferFunction;
    entries[4].binding = 4;
    entries[4].sampler = tfSampler;
    
    wgpu::BindGroupDescriptor desc = {};
    desc.label = "Raycasting Bind Group";
    desc.layout = pipeline.getBindGroupLayout(0);
    desc.entryCount = 5;
    desc.entries = entries;
    bindGroup = device.createBindGroup(desc);
    
    if (!bindGroup) {
        std::cout << "[ERROR]::RaycastingStage::InitBindGroup Failed to create bind group!" << std::endl;
        return false;
    }
    
    std::cout << "[VolumeRenderingTest] Raycasting pipeline created successfully!" << std::endl;
    return true;
}

void VolumeRenderingTest::RaycastingStage::Render(wgpu::RenderPassEncoder renderPass) {
    if (!pipeline || !bindGroup || !cubeVertexBuffer || !cubeIndexBuffer) return;
    
    renderPass.setPipeline(pipeline);
    renderPass.setBindGroup(0, bindGroup, 0, nullptr);
    renderPass.setVertexBuffer(0, cubeVertexBuffer, 0, WGPU_WHOLE_SIZE);
    renderPass.setIndexBuffer(cubeIndexBuffer, wgpu::IndexFormat::Uint16, 0, WGPU_WHOLE_SIZE);
    renderPass.drawIndexed(36, 1, 0, 0, 0);  // 渲染立方体（36个索引）
}

void VolumeRenderingTest::RaycastingStage::Release() {
    if (pipeline) {
        pipeline.release();
        pipeline = nullptr;
    }
    if (bindGroup) {
        bindGroup.release();
        bindGroup = nullptr;
    }
    if (volumeSampler) {
        volumeSampler.release();
        volumeSampler = nullptr;
    }
    if (tfSampler) {
        tfSampler.release();
        tfSampler = nullptr;
    }
    if (cubeVertexBuffer) {
        cubeVertexBuffer.release();
        cubeVertexBuffer = nullptr;
    }
    if (cubeIndexBuffer) {
        cubeIndexBuffer.release();
        cubeIndexBuffer = nullptr;
    }
    if (uniformBuffer) {
        uniformBuffer.release();
        uniformBuffer = nullptr;
    }
}