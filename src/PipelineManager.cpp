#include "PipelineManager.h"

// ============= PipelineManager 实现 =============
PipelineManager& PipelineManager::getInstance() {
    static PipelineManager instance;
    return instance;
}

RenderPipelineBuilder PipelineManager::createRenderPipeline() {
    return RenderPipelineBuilder();
}

ComputePipelineBuilder PipelineManager::createComputePipeline() {
    return ComputePipelineBuilder();
}

// ============= RenderPipelineBuilder 实现 =============
RenderPipelineBuilder& RenderPipelineBuilder::setDevice(wgpu::Device device) {
    m_device = device;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setLabel(const std::string& label) {
    m_label = label;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setVertexEntry(const std::string& entry) {
    m_vertexEntry = entry;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setFragmentEntry(const std::string& entry) {
    m_fragmentEntry = entry;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setVertexShader(const std::string& path, const std::string& entry) {
    m_vertexShaderPath = path;
    m_vertexEntry = entry;
    m_useVertexShaderPath = true;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setFragmentShader(const std::string& path, const std::string& entry) {
    m_fragmentShaderPath = path;
    m_fragmentEntry = entry;
    m_useFragmentShaderPath = true;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setVertexShaderSource(const std::string& source, const std::string& entry) {
    m_vertexShaderSource = source;
    m_vertexEntry = entry;
    m_useVertexShaderPath = false;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setFragmentShaderSource(const std::string& source, const std::string& entry) {
    m_fragmentShaderSource = source;
    m_fragmentEntry = entry;
    m_useFragmentShaderPath = false;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setVertexLayout(const wgpu::VertexBufferLayout& layout) {
    m_vertexLayout = layout;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setSwapChainFormat(wgpu::TextureFormat format) {
    m_swapChainFormat = format;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setPrimitiveTopology(wgpu::PrimitiveTopology topology) {
    m_topology = topology;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setCullMode(wgpu::CullMode cullMode) {
    m_cullMode = cullMode;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setFrontFace(wgpu::FrontFace frontFace) {
    m_frontFace = frontFace;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setBlendState(const wgpu::BlendState& blendState) {
    m_blendState = blendState;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setAlphaBlending() {
    m_blendState = BlendPresets::alphaBlending();
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setAdditiveBlending() {
    m_blendState = BlendPresets::additiveBlending();
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setPremultipliedAlpha() {
    m_blendState = BlendPresets::premultipliedAlpha();
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::disableBlending() {
    m_blendState.reset();
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setDepthStencilState(const wgpu::DepthStencilState& depthState) {
    m_depthState = depthState;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setReadOnlyDepth(wgpu::TextureFormat format) {
    m_depthState = DepthPresets::readOnlyDepth(format);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setStandardDepth(wgpu::TextureFormat format) {
    m_depthState = DepthPresets::standardDepth(format);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setVolumeRenderingDepth(wgpu::TextureFormat format) {
    m_depthState = DepthPresets::volumeRenderingDepth(format);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::disableDepth() {
    m_depthState.reset();
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::setMultisample(uint32_t count, uint32_t mask, bool alphaToCoverage) {
    m_multisampleCount = count;
    m_multisampleMask = mask;
    m_alphaToCoverage = alphaToCoverage;
    return *this;
}

wgpu::RenderPipeline RenderPipelineBuilder::build() {
    if (!m_device) {
        std::cerr << "[ERROR] RenderPipelineBuilder: Device not set!" << std::endl;
        return nullptr;
    }
    
    if (!m_vertexLayout.has_value()) {
        std::cerr << "[ERROR] RenderPipelineBuilder: Vertex layout not set!" << std::endl;
        return nullptr;
    }
    
    auto& shaderMgr = ShaderManager::getInstance();
    
    // 加载着色器
    wgpu::ShaderModule vertexShader;
    wgpu::ShaderModule fragmentShader;
    
    if (m_useVertexShaderPath) {
        vertexShader = shaderMgr.loadShader(m_device, m_vertexShaderPath);
    } else {
        vertexShader = shaderMgr.createFromSource(m_device, m_vertexShaderSource, m_label + " Vertex Shader");
    }
    
    if (m_useFragmentShaderPath) {
        fragmentShader = shaderMgr.loadShader(m_device, m_fragmentShaderPath);
    } else {
        fragmentShader = shaderMgr.createFromSource(m_device, m_fragmentShaderSource, m_label + " Fragment Shader");
    }
    
    if (!vertexShader || !fragmentShader) {
        std::cerr << "[ERROR] RenderPipelineBuilder: Failed to load shaders!" << std::endl;
        if (vertexShader) vertexShader.release();
        if (fragmentShader) fragmentShader.release();
        return nullptr;
    }
    
    // 创建管线描述符
    wgpu::RenderPipelineDescriptor desc = {};
    desc.label = m_label.c_str();
    
    // 顶点阶段
    desc.vertex.module = vertexShader;
    desc.vertex.entryPoint = m_vertexEntry.c_str();
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &m_vertexLayout.value();
    
    // 片段阶段
    wgpu::FragmentState fragmentState = {};
    fragmentState.module = fragmentShader;
    fragmentState.entryPoint = m_fragmentEntry.c_str();
    
    // 颜色目标
    wgpu::ColorTargetState colorTarget = {};
    colorTarget.format = m_swapChainFormat;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;
    
    if (m_blendState.has_value()) {
        static wgpu::BlendState blendState;  // 需要静态存储
        blendState = m_blendState.value();
        colorTarget.blend = &blendState;
    }
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    desc.fragment = &fragmentState;
    
    // 深度模板
    if (m_depthState.has_value()) {
        static wgpu::DepthStencilState depthStencil;  // 需要静态存储
        depthStencil = m_depthState.value();
        desc.depthStencil = &depthStencil;
    }
    
    // 多重采样
    wgpu::MultisampleState multisample = {};
    multisample.count = m_multisampleCount;
    multisample.mask = m_multisampleMask;
    multisample.alphaToCoverageEnabled = m_alphaToCoverage;
    desc.multisample = multisample;
    
    // 图元
    desc.primitive.topology = m_topology;
    desc.primitive.cullMode = m_cullMode;
    desc.primitive.frontFace = m_frontFace;
    desc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    
    // 创建管线
    wgpu::RenderPipeline pipeline = m_device.createRenderPipeline(desc);
    
    // 清理着色器
    vertexShader.release();
    fragmentShader.release();
    
    if (!pipeline) {
        std::cerr << "[ERROR] RenderPipelineBuilder: Failed to create render pipeline: " << m_label << std::endl;
    } else {
        std::cout << "[PipelineBuilder] Created render pipeline: " << m_label << std::endl;
    }
    
    return pipeline;
}

// ============= ComputePipelineBuilder 实现 =============
ComputePipelineBuilder& ComputePipelineBuilder::setDevice(wgpu::Device device) {
    m_device = device;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::setLabel(const std::string& label) {
    m_label = label;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::setEntry(const std::string& entry) {
    m_entry = entry;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::addBindGroupLayout(const wgpu::BindGroupLayout& layout) {
    m_bindGroupLayouts.push_back(layout);
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::setExplicitLayout(bool useExplicit) {
    m_useExplicitLayout = useExplicit;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::setShader(const std::string& path, const std::string& entry) {
    m_shaderPath = path;
    m_entry = entry;
    m_useShaderPath = true;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::setShaderSource(const std::string& source, const std::string& entry) {
    m_shaderSource = source;
    m_entry = entry;
    m_useShaderPath = false;
    return *this;
}

wgpu::ComputePipeline ComputePipelineBuilder::build() {
    if (!m_device) {
        std::cerr << "[ERROR] ComputePipelineBuilder: Device not set!" << std::endl;
        return nullptr;
    }
    
    auto& shaderMgr = ShaderManager::getInstance();
    
    // 加载着色器
    wgpu::ShaderModule shader;
    if (m_useShaderPath) {
        shader = shaderMgr.loadShader(m_device, m_shaderPath);
    } else {
        shader = shaderMgr.createFromSource(m_device, m_shaderSource, m_label);
    }
    
    if (!shader) {
        std::cerr << "[ERROR] ComputePipelineBuilder: Failed to load compute shader!" << std::endl;
        return nullptr;
    }
    
    // 创建管线描述符
    wgpu::ComputePipelineDescriptor desc = {};
    desc.label = m_label.c_str();
    desc.compute.module = shader;
    desc.compute.entryPoint = m_entry.c_str();

    wgpu::PipelineLayout pipelineLayout = nullptr;
    
    if (m_useExplicitLayout && !m_bindGroupLayouts.empty()) {
        wgpu::PipelineLayoutDescriptor layoutDesc = {};
        layoutDesc.label = (m_label + " Layout").c_str();
        layoutDesc.bindGroupLayoutCount = m_bindGroupLayouts.size();
        layoutDesc.bindGroupLayouts = reinterpret_cast<const WGPUBindGroupLayout*>(m_bindGroupLayouts.data());
        
        pipelineLayout = m_device.createPipelineLayout(layoutDesc);
        if (!pipelineLayout) {
            std::cerr << "[ERROR] ComputePipelineBuilder: Failed to create pipeline layout!" << std::endl;
            shader.release();
            return nullptr;
        }
        
        desc.layout = pipelineLayout;
    }
    // 如果不使用显式布局，WebGPU会自动从着色器推断
    
    // 创建管线
    wgpu::ComputePipeline pipeline = m_device.createComputePipeline(desc);
    
    // 清理着色器
    shader.release();
    
    if (!pipeline) {
        std::cerr << "[ERROR] ComputePipelineBuilder: Failed to create compute pipeline: " << m_label << std::endl;
    } else {
        std::cout << "[PipelineBuilder] Created compute pipeline: " << m_label << std::endl;
    }
    
    return pipeline;
}