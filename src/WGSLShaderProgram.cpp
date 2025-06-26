#include "WGSLShaderProgram.h"
#include <webgpu/webgpu.hpp>


WGSLShaderProgram::WGSLShaderProgram(wgpu::Device device) : m_device(device) 
{
}
    
    
static std::string LoadWGSLSource(const std::string& path) 
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool WGSLShaderProgram::LoadShaders(const std::string& vertexShaderPath, const std::string& fragmentShaderPath)
{
    std::string vertexShaderSource = LoadWGSLSource(vertexShaderPath);
    std::string fragmentShaderSource = LoadWGSLSource(fragmentShaderPath);

    if (vertexShaderSource.empty() || fragmentShaderSource.empty()) return false;

   // Load vertex shader
    wgpu::ShaderModuleWGSLDescriptor vsWGSLDesc = {};
    vsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    vsWGSLDesc.chain.next = nullptr;
    vsWGSLDesc.code = vertexShaderSource.c_str();

    wgpu::ShaderModuleDescriptor vsDesc = {};
    vsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&vsWGSLDesc);
    vsDesc.label = "Vertex Shader";
    m_vertexShader = m_device.createShaderModule(vsDesc);

    // Load fragment shader
    wgpu::ShaderModuleWGSLDescriptor fsWGSLDesc = {};
    fsWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    fsWGSLDesc.chain.next = nullptr;
    fsWGSLDesc.code = fragmentShaderSource.c_str();

    wgpu::ShaderModuleDescriptor fsDesc = {};
    fsDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&fsWGSLDesc);
    fsDesc.label = "Fragment Shader";
    m_fragmentShader = m_device.createShaderModule(fsDesc);

    if (!m_vertexShader || !m_fragmentShader) {
        std::cerr << "Failed to create shader modules." << std::endl;
        return false;
    }

    return true;
}

void WGSLShaderProgram::CreatePipeline(wgpu::TextureFormat swapChainFormat,
                                       const wgpu::BindGroupLayout& bindGroupLayout,
                                       const wgpu::VertexBufferLayout& vertexLayout)
{
    // Create pipeline layout
    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = "WGSL Pipeline Layout";
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    WGPUBindGroupLayout rawLayout = bindGroupLayout;
    pipelineLayoutDesc.bindGroupLayouts = &rawLayout;
    wgpu::PipelineLayout pipelineLayout = m_device.createPipelineLayout(pipelineLayoutDesc);

    // Set up vertex stage
    wgpu::VertexState vertexState = {};
    vertexState.module = m_vertexShader;
    vertexState.entryPoint = "vs_main";
    vertexState.bufferCount = 1;
    vertexState.buffers = &vertexLayout;

    // Set up fragment stage
    wgpu::ColorTargetState colorTarget = {};
    colorTarget.format = swapChainFormat;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState = {};
    fragmentState.module = m_fragmentShader;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    // Create render pipeline descriptor
    wgpu::RenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = "WGSL Render Pipeline";
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex = vertexState;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;
    pipelineDesc.depthStencil = nullptr; // No depth/stencil for now
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_pipeline = m_device.createRenderPipeline(pipelineDesc);
}

bool WGSLShaderProgram::LoadComputeShader(const std::string& computeShaderPath)
{
    std::string computeShaderSource = LoadWGSLSource(computeShaderPath);
    if (computeShaderSource.empty()) return false;

    wgpu::ShaderModuleWGSLDescriptor csWGSLDesc = {};
    csWGSLDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    csWGSLDesc.chain.next = nullptr;
    csWGSLDesc.code = computeShaderSource.c_str();

    wgpu::ShaderModuleDescriptor csDesc = {};
    csDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&csWGSLDesc);
    csDesc.label = "Compute Shader";
    m_computeShader = m_device.createShaderModule(csDesc);

    if (!m_computeShader) {
        std::cerr << "Failed to create compute shader module." << std::endl;
        return false;
    }

    return true;
}

void WGSLShaderProgram::CreateComputePipeline(const wgpu::BindGroupLayout& bindGroupLayout1, const wgpu::BindGroupLayout& bindGroupLayout2)
{
    if (!m_computeShader) {
        std::cerr << "Compute shader not loaded." << std::endl;
        return;
    }
    // Create pipeline layout
    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = "WGSL Compute Pipeline Layout";
    pipelineLayoutDesc.bindGroupLayoutCount = 1;

    WGPUBindGroupLayout raw = bindGroupLayout1;

    WGPUBindGroupLayout rawLayouts[2] = {
        static_cast<WGPUBindGroupLayout>(bindGroupLayout1),  // Group 0
        static_cast<WGPUBindGroupLayout>(bindGroupLayout2)   // Group 1
    };
    pipelineLayoutDesc.bindGroupLayouts = &raw;
    wgpu::PipelineLayout pipelineLayout = m_device.createPipelineLayout(pipelineLayoutDesc);

    // Set up compute stage
    wgpu::ComputePipelineDescriptor computeDesc = {};
    computeDesc.label = "WGSL Compute Pipeline";
    computeDesc.layout = pipelineLayout;
    computeDesc.compute.module = m_computeShader;
    computeDesc.compute.entryPoint = "cs_main";
    m_computePipeline = m_device.createComputePipeline(computeDesc);

    if (!m_computePipeline) {
        std::cerr << "Failed to create compute pipeline." << std::endl;
        return;
    }

    std::cout << "Compute pipeline created successfully." << std::endl;
}

