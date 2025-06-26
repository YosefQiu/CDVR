#include "ComputeOptimizedVisualizer.h"

void ComputeOptimizedVisualizer::CreatePipeline(wgpu::TextureFormat swapChainFormat) {
    // 创建计算资源
    CreateComputeResources();
    
    // 创建数据纹理
    CreateDataTexture();
    
    // 创建简化的渲染管线
    CreateSimplifiedRenderPipeline(swapChainFormat);
    
    UpdateTransferFunction(nullptr, nullptr);
    UpdateDataTexture();
}

void ComputeOptimizedVisualizer::CreateComputeResources() 
{
    // 1. 使用 WGSLShaderProgram 加载 compute shader
    m_computeShaderProgram = std::make_unique<WGSLShaderProgram>(m_device);
    
    if (!m_computeShaderProgram->LoadComputeShader("../shaders/sparse_data.comp.wgsl")) {
        std::cerr << "Failed to load compute shader!" << std::endl;
        return;
    }
    
    // 2. 创建 Compute Uniform Buffer
    m_computeUniforms.minValue = m_uniforms.minValue;
    m_computeUniforms.maxValue = m_uniforms.maxValue;
    m_computeUniforms.gridWidth = static_cast<float>(m_header.width);
    m_computeUniforms.gridHeight = static_cast<float>(m_header.height);
    m_computeUniforms.numPoints = static_cast<uint32_t>(m_sparsePoints.size());
    m_computeUniforms.searchRadius = 5.0f;
    
    wgpu::BufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.size = sizeof(ComputeUniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    m_computeUniformBuffer = m_device.createBuffer(uniformBufferDesc);
    m_queue.writeBuffer(m_computeUniformBuffer, 0, &m_computeUniforms, sizeof(ComputeUniforms));
    
    // 3. 创建 Compute Bind Group Layout
    std::vector<wgpu::BindGroupLayoutEntry> computeLayoutEntries(3);
    
    // Uniform buffer
    computeLayoutEntries[0].binding = 0;
    computeLayoutEntries[0].visibility = wgpu::ShaderStage::Compute;
    computeLayoutEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    
    // Storage buffer (稀疏点数据)
    computeLayoutEntries[1].binding = 1;
    computeLayoutEntries[1].visibility = wgpu::ShaderStage::Compute;
    computeLayoutEntries[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    
    // Storage texture (输出)
    computeLayoutEntries[2].binding = 2;
    computeLayoutEntries[2].visibility = wgpu::ShaderStage::Compute;
    computeLayoutEntries[2].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
    computeLayoutEntries[2].storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
    computeLayoutEntries[2].storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;
    
    wgpu::BindGroupLayoutDescriptor computeLayoutDesc{};
    computeLayoutDesc.entryCount = computeLayoutEntries.size();
    computeLayoutDesc.entries = computeLayoutEntries.data();
    m_computeBindGroupLayout = m_device.createBindGroupLayout(computeLayoutDesc);
    
    // 4. 创建 Transfer Function Bind Group Layout (Group 1)
    std::vector<wgpu::BindGroupLayoutEntry> tfLayoutEntries(1);

    tfLayoutEntries[0].binding = 0;
    tfLayoutEntries[0].visibility = wgpu::ShaderStage::Compute;
    tfLayoutEntries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    tfLayoutEntries[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;
    tfLayoutEntries[0].texture.multisampled = false;
    // 设置其他类型为未定义
    tfLayoutEntries[0].buffer.type = wgpu::BufferBindingType::Undefined;
    tfLayoutEntries[0].sampler.type = wgpu::SamplerBindingType::Undefined;
    tfLayoutEntries[0].storageTexture.access = wgpu::StorageTextureAccess::Undefined;

    wgpu::BindGroupLayoutDescriptor tfLayoutDesc{};
    tfLayoutDesc.entryCount = tfLayoutEntries.size();
    tfLayoutDesc.entries = tfLayoutEntries.data();
    m_transferFunctionBindGroupLayout = m_device.createBindGroupLayout(tfLayoutDesc);


    // 5. 使用 WGSLShaderProgram 创建 Compute Pipeline
    m_computeShaderProgram->CreateComputePipeline(m_computeBindGroupLayout, m_transferFunctionBindGroupLayout);
    m_computePipeline = m_computeShaderProgram->GetComputePipeline();
    
    if (!m_computePipeline) {
        std::cerr << "ERROR: GetComputePipeline returned null!" << std::endl;
    } else {
        std::cout << "✓ Pipeline created successfully" << std::endl;
    }
}

// 修改 CreateSimplifiedRenderPipeline
void ComputeOptimizedVisualizer::CreateSimplifiedRenderPipeline(wgpu::TextureFormat format) 
{

    // === 1. 创建 Bind Group Layout ===
    wgpu::BindGroupLayoutEntry entries[3] = {};

    // Binding 0: Uniforms
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    entries[0].buffer.hasDynamicOffset = false;
    entries[0].buffer.minBindingSize = sizeof(Uniforms);

    // Binding 1: Texture
    entries[1].binding = 1;
    entries[1].visibility = wgpu::ShaderStage::Fragment;
    entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
    entries[1].texture.viewDimension = wgpu::TextureViewDimension::_2D;
    entries[1].texture.multisampled = false;

    // Binding 2: Sampler
    entries[2].binding = 2;
    entries[2].visibility = wgpu::ShaderStage::Fragment;
    entries[2].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor layoutDesc{};
    layoutDesc.label = "Texture BindGroupLayout";
    layoutDesc.entryCount = 3;
    layoutDesc.entries = entries;

    m_bindGroupLayout = m_device.createBindGroupLayout(layoutDesc);

    // === 2. 创建 Bind Group ===
    wgpu::BindGroupEntry bindEntries[3] = {};

    // Binding 0: Uniforms
    bindEntries[0].binding = 0;
    bindEntries[0].buffer = m_uniformBuffer;
    bindEntries[0].offset = 0;
    bindEntries[0].size = sizeof(Uniforms);

    // Binding 1: Texture
    bindEntries[1].binding = 1;
    bindEntries[1].textureView = m_dataTextureView;

    // Binding 2: Sampler
    bindEntries[2].binding = 2;
    bindEntries[2].sampler = m_dataSampler;

    wgpu::BindGroupDescriptor bindDesc{};
    bindDesc.label = "Texture BindGroup";
    bindDesc.layout = m_bindGroupLayout;
    bindDesc.entryCount = 3;
    bindDesc.entries = bindEntries;

    m_bindGroup = m_device.createBindGroup(bindDesc);
    m_renderBindGroup = m_bindGroup;

    // === 3. 创建渲染管线 ===
    wgpu::VertexBufferLayout vertexBufferLayout = CreateVertexLayout();

    m_shaderProgram = std::make_unique<WGSLShaderProgram>(m_device);
    bool success = m_shaderProgram->LoadShaders(
        "../shaders/sparse_data.vert.wgsl",
        "../shaders/sparse_data.frag2.wgsl");
    if (!success) {
        std::cerr << "Failed to load shaders!" << std::endl;
        return;
    }

    m_shaderProgram->CreatePipeline(format, m_bindGroupLayout, vertexBufferLayout);
    m_simplifiedRenderPipeline = m_shaderProgram->GetPipeline();

    std::cout << "Simplified render pipeline created" << std::endl;
}

void ComputeOptimizedVisualizer::CreateDataTexture() 
{
    // 基于数据密度选择合适的纹理分辨率
    float dataAspectRatio = static_cast<float>(m_header.width) / m_header.height; // 150/450 = 0.333
    
    // 使用固定分辨率，保持数据宽高比
    uint32_t baseSize = 1024;
    uint32_t textureWidth = baseSize;
    uint32_t textureHeight = static_cast<uint32_t>(baseSize / dataAspectRatio);
    
    // 确保是 8 的倍数（为了工作组对齐）
    textureWidth = (textureWidth + 7) & ~7;
    textureHeight = (textureHeight + 7) & ~7;
    
    std::cout << "Creating data texture with resolution: " << textureWidth << "x" << textureHeight << std::endl;
    std::cout << "Data size: " << m_header.width << "x" << m_header.height << std::endl;
    
    // 创建纹理
    wgpu::TextureDescriptor textureDesc{};
    textureDesc.size = {textureWidth, textureHeight, 1};
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.dimension = wgpu::TextureDimension::_2D;
    textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
    
    m_dataTexture = m_device.createTexture(textureDesc);
    m_dataTextureView = m_dataTexture.createView();
    
    // 创建采样器
    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.magFilter = wgpu::FilterMode::Linear;
    samplerDesc.minFilter = wgpu::FilterMode::Linear;
    samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    samplerDesc.maxAnisotropy = 1; 
    m_dataSampler = m_device.createSampler(samplerDesc);
    
    // 更新 compute uniforms 中的搜索半径
    float texelSizeInDataSpace = std::max(
        m_header.width / static_cast<float>(textureWidth),
        m_header.height / static_cast<float>(textureHeight)
    );
    
    m_computeUniforms.searchRadius = std::max(5.0f, texelSizeInDataSpace * 3.0f);
    
    // 创建 Compute Bind Group
    std::vector<wgpu::BindGroupEntry> computeBindEntries(3);
    
    computeBindEntries[0].binding = 0;
    computeBindEntries[0].buffer = m_computeUniformBuffer;
    computeBindEntries[0].size = sizeof(ComputeUniforms);
    
    computeBindEntries[1].binding = 1;
    computeBindEntries[1].buffer = m_storageBuffer;
    computeBindEntries[1].size = m_sparsePoints.size() * 16; // 16 bytes per point
    
    computeBindEntries[2].binding = 2;
    computeBindEntries[2].textureView = m_dataTextureView;
    
    wgpu::BindGroupDescriptor computeBindGroupDesc{};
    computeBindGroupDesc.layout = m_computeBindGroupLayout;
    computeBindGroupDesc.entryCount = computeBindEntries.size();
    computeBindGroupDesc.entries = computeBindEntries.data();
    m_computeBindGroup = m_device.createBindGroup(computeBindGroupDesc);
    
    std::cout << "Data texture created: " << textureWidth << "x" << textureHeight << std::endl;
}

void ComputeOptimizedVisualizer::UpdateDataTexture() {
    if (!m_computePipeline || !m_computeBindGroup) {
        std::cerr << "Compute pipeline or bind group not initialized!" << std::endl;
        return;
    }
    
    wgpu::CommandEncoder encoder = m_device.createCommandEncoder();
    wgpu::ComputePassEncoder computePass = encoder.beginComputePass();
    
    computePass.setPipeline(m_computePipeline);
    computePass.setBindGroup(0, m_computeBindGroup, 0, nullptr);
    computePass.setBindGroup(1, m_transferFunctionBindGroup, 0, nullptr); // 设置 transfer function bind group
    
    // 计算工作组数量
    uint32_t textureWidth = m_dataTexture.getWidth();
    uint32_t textureHeight = m_dataTexture.getHeight();
    uint32_t workgroupsX = (textureWidth + 7) / 8;  // 向上取整
    uint32_t workgroupsY = (textureHeight + 7) / 8;
    
    computePass.dispatchWorkgroups(workgroupsX, workgroupsY, 1);
    computePass.end();
    
    wgpu::CommandBuffer commands = encoder.finish();
    m_queue.submit(1, &commands);
    
    std::cout << "Data texture updated with " << workgroupsX << "x" << workgroupsY << " workgroups" << std::endl;
}

void ComputeOptimizedVisualizer::Render(wgpu::RenderPassEncoder renderPass) {
    if (!m_simplifiedRenderPipeline || !m_renderBindGroup) {
        return;
    }
    
    renderPass.setPipeline(m_simplifiedRenderPipeline);
    renderPass.setBindGroup(0, m_renderBindGroup, 0, nullptr);  // 设置 bind group
    renderPass.setVertexBuffer(0, m_vertexBuffer, 0, 4 * 4 * sizeof(float));  // 4个顶点，每个4个float
    renderPass.draw(4, 1, 0, 0);  // 4个顶点，三角形带

    // 更新 uniforms
    UpdateUniforms(static_cast<float>(m_windowWidth) / m_windowHeight);
    
}

void ComputeOptimizedVisualizer::OnWindowResize(int width, int height) {
    SparseDataVisualizer::OnWindowResize(width, height);
    
    // 重新创建纹理和相关资源
    CreateDataTexture();
    UpdateDataTexture();
}


void ComputeOptimizedVisualizer::UpdateTransferFunction(wgpu::TextureView tfTextureView, wgpu::Sampler tfSampler)
{
    std::cout << "=== UpdateTransferFunction Debug ===" << std::endl;

    wgpu::TextureView activeTextureView;


    // 如果传入了纹理视图和采样器，直接使用它们
    if (tfTextureView)
    {
        std::cout << "Using provided transfer function texture view." << std::endl;
        
        // === 测试：创建全黄纹理 ===
        std::cout << "🧪 Creating test yellow texture..." << std::endl;
        
        // 创建全黄色数据
        const uint32_t testWidth = 256;
        std::vector<uint8_t> yellowData(testWidth * 4);
        for (uint32_t i = 0; i < testWidth; i++) {
            yellowData[i * 4 + 0] = 255;  // R = 255
            yellowData[i * 4 + 1] = 255;  // G = 255  
            yellowData[i * 4 + 2] = 0;    // B = 0 (红+绿=黄)
            yellowData[i * 4 + 3] = 255;  // A = 255
        }
        
        // 创建测试纹理
        wgpu::TextureDescriptor testTextureDesc{};
        testTextureDesc.size = {testWidth, 1, 1};
        testTextureDesc.mipLevelCount = 1;
        testTextureDesc.sampleCount = 1;
        testTextureDesc.dimension = wgpu::TextureDimension::_2D;
        testTextureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        testTextureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
        testTextureDesc.label = "Test Yellow Texture";
        
        wgpu::Texture testTexture = m_device.createTexture(testTextureDesc);
        
        // 上传黄色数据
        wgpu::ImageCopyTexture imageCopy{};
        imageCopy.texture = testTexture;
        imageCopy.mipLevel = 0;
        imageCopy.origin = {0, 0, 0};
        imageCopy.aspect = wgpu::TextureAspect::All;
        
        wgpu::TextureDataLayout dataLayout{};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = testWidth * 4;
        dataLayout.rowsPerImage = 1;
        
        m_queue.writeTexture(imageCopy, yellowData.data(), yellowData.size(),
                            dataLayout, {testWidth, 1, 1});
        
        // 创建测试纹理视图
        wgpu::TextureViewDescriptor testViewDesc{};
        testViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        testViewDesc.dimension = wgpu::TextureViewDimension::_2D;
        testViewDesc.baseMipLevel = 0;
        testViewDesc.mipLevelCount = 1;
        testViewDesc.baseArrayLayer = 0;
        testViewDesc.arrayLayerCount = 1;
        testViewDesc.label = "Test Yellow Texture View";
        
        wgpu::TextureView testTextureView = testTexture.createView(testViewDesc);
        
        std::cout << "✅ Test yellow texture created and uploaded" << std::endl;
        std::cout << "   Using YELLOW texture instead of widget texture" << std::endl;
        activeTextureView = testTextureView;  // 使用测试纹理视图
    } 
    else 
    {
        std::cout << "Creating simulated transfer function texture." << std::endl;
        const uint32_t tfWidth = 256;
        const uint32_t tfHeight = 1;

        // 创建颜色数据
        std::vector<uint8_t> colorData(tfWidth * tfHeight * 4);
        for (uint32_t i = 0; i < tfWidth; i++) {
            float t = static_cast<float>(i) / static_cast<float>(tfWidth - 1);
            
            uint8_t r, g, b, a = 255;
            
            // 简化的热力图：红->绿->蓝
            if (t < 0.5f) {
                r = 255;
                g = static_cast<uint8_t>(t * 3.0f * 255);
                b = 0;
            } else if (t < 0.66f) {
                r = static_cast<uint8_t>((0.66f - t) * 3.0f * 255);
                g = 128;
                b = static_cast<uint8_t>((t - 0.33f) * 3.0f * 255);
            } else {
                r = 0;
                g = static_cast<uint8_t>((1.0f - t) * 3.0f * 255);
                b = 255;
            }
            
            uint32_t pixelIndex = i * 4;
            colorData[pixelIndex + 0] = r;
            colorData[pixelIndex + 1] = g;
            colorData[pixelIndex + 2] = b;
            colorData[pixelIndex + 3] = a;
        }

        // 创建纹理描述符
        wgpu::TextureDescriptor tfTextureDesc{};
        tfTextureDesc.size = {tfWidth, tfHeight, 1};
        tfTextureDesc.mipLevelCount = 1;
        tfTextureDesc.sampleCount = 1;
        tfTextureDesc.dimension = wgpu::TextureDimension::_2D;
        tfTextureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        tfTextureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
        tfTextureDesc.label = "Simulated Transfer Function";

        // *** 关键：保存为成员变量 ***
        m_DefaultTFTexture = m_device.createTexture(tfTextureDesc);

        // 上传数据
        wgpu::ImageCopyTexture imageCopyTexture{};
        imageCopyTexture.texture = m_DefaultTFTexture;
        imageCopyTexture.mipLevel = 0;
        imageCopyTexture.origin = {0, 0, 0};
        imageCopyTexture.aspect = wgpu::TextureAspect::All;

        wgpu::TextureDataLayout textureDataLayout{};
        textureDataLayout.offset = 0;
        textureDataLayout.bytesPerRow = tfWidth * 4;
        textureDataLayout.rowsPerImage = tfHeight;

        m_queue.writeTexture(imageCopyTexture, colorData.data(), colorData.size(), 
                            textureDataLayout, {tfWidth, tfHeight, 1});

        // 创建纹理视图
        wgpu::TextureViewDescriptor tfViewDesc{};
        tfViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        tfViewDesc.dimension = wgpu::TextureViewDimension::_2D;
        tfViewDesc.baseMipLevel = 0;
        tfViewDesc.mipLevelCount = 1;
        tfViewDesc.baseArrayLayer = 0;
        tfViewDesc.arrayLayerCount = 1;
        tfViewDesc.label = "Default TF View";

        m_DefaultTFTextureView = m_DefaultTFTexture.createView(tfViewDesc);

        activeTextureView = m_DefaultTFTextureView; 
    }
    
    
    // 检查布局是否有效
    if (!m_transferFunctionBindGroupLayout) {
        std::cerr << "ERROR: Transfer function bind group layout is null!" << std::endl;
        return;
    }

    // 创建绑定组
    m_transferFunctionBindGroup = nullptr; 
    std::vector<wgpu::BindGroupEntry> tfEntries(1);
    tfEntries[0].binding = 0;
    tfEntries[0].textureView = activeTextureView;
    tfEntries[0].buffer = nullptr;
    tfEntries[0].sampler = nullptr;
    tfEntries[0].offset = 0;
    tfEntries[0].size = 0;
    
    wgpu::BindGroupDescriptor tfBindGroupDesc{};
    tfBindGroupDesc.layout = m_transferFunctionBindGroupLayout;
    tfBindGroupDesc.entryCount = tfEntries.size();
    tfBindGroupDesc.entries = tfEntries.data();
    tfBindGroupDesc.label = "Transfer Function Bind Group";
    
    m_transferFunctionBindGroup = m_device.createBindGroup(tfBindGroupDesc);

    // auto oldBindGroup = m_transferFunctionBindGroup;
    // m_transferFunctionBindGroup = m_device.createBindGroup(tfBindGroupDesc);
    
    // std::cout << "Old bind group pointer: " << (void*)oldBindGroup.Get() << std::endl;
    // std::cout << "New bind group pointer: " << (void*)m_transferFunctionBindGroup.Get() << std::endl;
    
    // if (oldBindGroup.Get() == m_transferFunctionBindGroup.Get()) {
    //     std::cout << "❌ ERROR: Bind group pointer didn't change!" << std::endl;
    // } else {
    //     std::cout << "✅ Bind group successfully updated" << std::endl;
    // }
    
   
}