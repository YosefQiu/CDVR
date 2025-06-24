#include "ComputeOptimizedVisualizer.h"

void ComputeOptimizedVisualizer::CreatePipeline(wgpu::TextureFormat swapChainFormat) {
    // 创建计算资源
    CreateComputeResources();
    
    // 创建数据纹理
    CreateDataTexture();
    
    // 创建简化的渲染管线
    CreateSimplifiedRenderPipeline(swapChainFormat);
    
    // 第一次更新纹理
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
    
    // 4. 使用 WGSLShaderProgram 创建 Compute Pipeline
    m_computeShaderProgram->CreateComputePipeline(m_computeBindGroupLayout);
    m_computePipeline = m_computeShaderProgram->GetComputePipeline();
    
    std::cout << "Compute pipeline created successfully using WGSLShaderProgram" << std::endl;
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
    uint32_t baseSize = 512;
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