#include "SparseDataVisualizer.h"
#include <iostream>
#include <fstream>  // 添加这个头文件
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

SparseDataVisualizer::SparseDataVisualizer(wgpu::Device device, wgpu::Queue queue)
    : m_device(device), m_queue(queue) {
}

SparseDataVisualizer::~SparseDataVisualizer() {
    // WebGPU资源会自动释放
}

bool SparseDataVisualizer::LoadFromBinary(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }
    
    // 读取header
    file.read(reinterpret_cast<char*>(&m_header), sizeof(DataHeader));
    
    std::cout << "Loading sparse data:" << std::endl;
    std::cout << "  Grid size: " << m_header.width << " x " << m_header.height << std::endl;
    std::cout << "  Number of points: " << m_header.numPoints << std::endl;
    
    // 读取稀疏点数据
    m_sparsePoints.resize(m_header.numPoints);
    file.read(reinterpret_cast<char*>(m_sparsePoints.data()), 
              m_header.numPoints * sizeof(SparsePoint));
    
    file.close();
    
    // 计算值的范围（用于颜色映射）
    ComputeValueRange();
    
    return true;
}

void SparseDataVisualizer::ComputeValueRange() {
    m_uniforms.minValue = std::numeric_limits<float>::max();
    m_uniforms.maxValue = std::numeric_limits<float>::lowest();
    
    for (const auto& point : m_sparsePoints) {
        m_uniforms.minValue = std::min(m_uniforms.minValue, point.value);
        m_uniforms.maxValue = std::max(m_uniforms.maxValue, point.value);
    }
    
    std::cout << "Value range: [" << m_uniforms.minValue << ", " 
              << m_uniforms.maxValue << "]" << std::endl;
}


void SparseDataVisualizer::UpdateUniforms(float aspectRatio) {
    // 设置简单的正交投影
    // 使用单位矩阵作为视图矩阵
    float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    // 简单的正交投影矩阵
    float ortho[16] = {
        1.0f / aspectRatio, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    
    
    memcpy(m_uniforms.viewMatrix, identity, sizeof(float) * 16);
    memcpy(m_uniforms.projMatrix, ortho, sizeof(float) * 16);
    
    // 确保其他 uniform 值已设置
    m_uniforms.gridWidth = static_cast<float>(m_header.width);
    m_uniforms.gridHeight = static_cast<float>(m_header.height);
    // minValue 和 maxValue 已经在 ComputeValueRange() 中设置
    
    m_queue.writeBuffer(m_uniformBuffer, 0, &m_uniforms, sizeof(Uniforms));
}

void SparseDataVisualizer::OnWindowResize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    
    float dataAspect = static_cast<float>(m_header.width) / static_cast<float>(m_header.height);
    float windowAspect = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    
    if (windowAspect > dataAspect) {
        // 窗口更宽，需要在 X 方向上缩小
        scaleX = dataAspect / windowAspect;
    } else {
        // 窗口更高，需要在 Y 方向上缩小
        scaleY = windowAspect / dataAspect;
    }
    
    // 更新顶点数据
    float vertices[] = {
        // positions                    // texCoords
        -scaleX, -scaleY,              0.0f, 0.0f,
         scaleX, -scaleY,              1.0f, 0.0f,
        -scaleX,  scaleY,              0.0f, 1.0f,
         scaleX,  scaleY,              1.0f, 1.0f,
    };
    
    // 更新顶点缓冲
    m_queue.writeBuffer(m_vertexBuffer, 0, vertices, sizeof(vertices));
    
    // std::cout << "Window resized to " << width << "x" << height << std::endl;
    // std::cout << "Updated scale: " << scaleX << " x " << scaleY << std::endl;
    // 更新uniforms
    UpdateUniforms(windowAspect);
}

void SparseDataVisualizer::CreatePipeline(wgpu::TextureFormat swapChainFormat) {
    // 完整的稀疏数据可视化shader
    const char* shaderSource = R"(
struct Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    gridWidth: f32,
    gridHeight: f32,
    minValue: f32,
    maxValue: f32,
}

struct SparsePoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> sparsePoints: array<SparsePoint>;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoords: vec2<f32>,
}

@vertex
fn vs_main(@location(0) pos: vec2<f32>, @location(1) texCoords: vec2<f32>) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4<f32>(pos, 0.0, 1.0);
    output.texCoords = texCoords;
    return output;
}

// 最近邻插值
fn findNearestValue(coord: vec2<f32>) -> f32 {
    let numPoints = arrayLength(&sparsePoints);
    var minDist = 999999.0;
    var nearestValue = 0.0;
    
    for (var i = 0u; i < numPoints; i = i + 1u) {
        let point = sparsePoints[i];
        let dist = distance(coord, vec2<f32>(point.x, point.y));
        if (dist < minDist) {
            minDist = dist;
            nearestValue = point.value;
        }
    }
    
    return nearestValue;
}

// 颜色映射函数 (coolwarm colormap)
fn coolwarmColormap(t: f32) -> vec3<f32> {
    let t_clamped = clamp(t, 0.0, 1.0);
    
    // 简化的coolwarm颜色映射
    if (t_clamped < 0.5) {
        // 蓝色到白色
        let s = t_clamped * 2.0;
        return mix(vec3<f32>(0.0, 0.0, 1.0), vec3<f32>(1.0, 1.0, 1.0), s);
    } else {
        // 白色到红色
        let s = (t_clamped - 0.5) * 2.0;
        return mix(vec3<f32>(1.0, 1.0, 1.0), vec3<f32>(1.0, 0.0, 0.0), s);
    }
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // 将纹理坐标转换为网格坐标
    let gridCoord = vec2<f32>(
        input.texCoords.x * uniforms.gridWidth,
        input.texCoords.y * uniforms.gridHeight
    );
    
    // 最近邻插值
    let value = findNearestValue(gridCoord);
    
    // 归一化到[0,1]
    let normalized = (value - uniforms.minValue) / 
                     (uniforms.maxValue - uniforms.minValue);
    
    // 应用颜色映射
    let color = coolwarmColormap(normalized);
    
    return vec4<f32>(color, 1.0);
}
)";

    // 使用 C API
    WGPUShaderModuleWGSLDescriptor wgslDescriptor = {};
    wgslDescriptor.chain.next = nullptr;
    wgslDescriptor.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDescriptor.code = shaderSource;
    
    WGPUShaderModuleDescriptor descriptor = {};
    descriptor.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgslDescriptor);
    descriptor.label = "Sparse Data Shader";
    
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_device, &descriptor);
    
    if (!shaderModule) {
        std::cerr << "Failed to create shader module!" << std::endl;
        return;
    }
    
    std::cout << "Shader module created successfully" << std::endl;
    
    // 创建 bind group layout
    wgpu::BindGroupLayoutEntry bindGroupLayoutEntries[2] = {};
    
    // Uniform buffer binding
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindGroupLayoutEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bindGroupLayoutEntries[0].buffer.hasDynamicOffset = false;
    bindGroupLayoutEntries[0].buffer.minBindingSize = 0;
    
    // Storage buffer binding
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = wgpu::ShaderStage::Fragment;
    bindGroupLayoutEntries[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    bindGroupLayoutEntries[1].buffer.hasDynamicOffset = false;
    bindGroupLayoutEntries[1].buffer.minBindingSize = 0;
    
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.label = "Sparse Data Bind Group Layout";
    bindGroupLayoutDesc.entryCount = 2;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    
    m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
    
    // 创建 bind group
    wgpu::BindGroupEntry bindGroupEntries[2] = {};
    
    bindGroupEntries[0].binding = 0;
    bindGroupEntries[0].buffer = m_uniformBuffer;
    bindGroupEntries[0].offset = 0;
    bindGroupEntries[0].size = sizeof(Uniforms);
    
    bindGroupEntries[1].binding = 1;
    bindGroupEntries[1].buffer = m_storageBuffer;
    bindGroupEntries[1].offset = 0;
    bindGroupEntries[1].size = m_sparsePoints.size() * 16;  // 16 bytes per point (with padding)
    
    wgpu::BindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.label = "Sparse Data Bind Group";
    bindGroupDesc.layout = m_bindGroupLayout;
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    
    m_bindGroup = m_device.createBindGroup(bindGroupDesc);
    
    // 创建顶点布局（位置 + 纹理坐标）
    WGPUVertexAttribute vertexAttributes[2] = {};
    vertexAttributes[0].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[0].offset = 0;
    vertexAttributes[0].shaderLocation = 0;
    
    vertexAttributes[1].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[1].offset = 2 * sizeof(float);
    vertexAttributes[1].shaderLocation = 1;
    
    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = 4 * sizeof(float);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = vertexAttributes;
    
    // Pipeline layout（使用 bind group layout）
    wgpu::PipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.label = "Sparse Data Pipeline Layout";
    layoutDesc.bindGroupLayoutCount = 1;
    // 创建一个数组来存储 C API 指针
    std::vector<WGPUBindGroupLayout> bindGroupLayouts = { m_bindGroupLayout };
    layoutDesc.bindGroupLayouts = bindGroupLayouts.data();
    wgpu::PipelineLayout pipelineLayout = m_device.createPipelineLayout(layoutDesc);
    
    // Render pipeline
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = "Simple Pipeline";
    pipelineDesc.layout = pipelineLayout;
    
    // Vertex stage
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    
    // Fragment stage
    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = swapChainFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    pipelineDesc.fragment = &fragmentState;
    
    // Primitive state
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    
    // Depth stencil state
    pipelineDesc.depthStencil = nullptr;
    
    // Multisample state
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
    
    if (!m_pipeline) {
        std::cerr << "Failed to create render pipeline!" << std::endl;
    } else {
        std::cout << "Pipeline created successfully" << std::endl;
    }
    
    // 清理
    wgpuShaderModuleRelease(shaderModule);
    wgpuPipelineLayoutRelease(pipelineLayout);
}

// 更新 CreateBuffers 恢复全屏四边形

void SparseDataVisualizer::CreateBuffers(int windowWidth, int windowHeight) 
{
    m_windowWidth = windowWidth;
    m_windowHeight = windowHeight;

    // 计算正确的宽高比
    float dataAspect = static_cast<float>(m_header.width) / static_cast<float>(m_header.height);  // 150/450 = 0.333
    float windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);  // 约 1.68

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    
    if (windowAspect > dataAspect) {
        // 窗口更宽，需要在 X 方向上缩小
        scaleX = dataAspect / windowAspect;
    } else {
        // 窗口更高，需要在 Y 方向上缩小
        scaleY = windowAspect / dataAspect;
    }
    
    // 1. 创建保持宽高比的四边形顶点
    float vertices[] = {
        // positions                  // texCoords
        -scaleX,    -scaleY,    0.0f,   0.0f,
        scaleX,     -scaleY,    1.0f,   0.0f,
        -scaleX,    scaleY,     0.0f,  1.0f,
        scaleX,    scaleY,    1.0f,  1.0f,
    };
    
    wgpu::BufferDescriptor vertexBufferDesc{};
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    vertexBufferDesc.mappedAtCreation = false;
    m_vertexBuffer = m_device.createBuffer(vertexBufferDesc);
    m_queue.writeBuffer(m_vertexBuffer, 0, vertices, sizeof(vertices));
    
    std::cout << "Vertex buffer created with aspect correction" << std::endl;
    std::cout << "Data aspect: " << dataAspect << ", Window aspect: " << windowAspect << std::endl;
    std::cout << "Scale: " << scaleX << " x " << scaleY << std::endl;
    
    // 2. 创建存储缓冲，存放稀疏点数据
    if (!m_sparsePoints.empty()) {
        // 确保每个点是16字节对齐的
        struct AlignedSparsePoint {
            float x, y, value, padding;
        };
        
        std::vector<AlignedSparsePoint> alignedPoints(m_sparsePoints.size());
        for (size_t i = 0; i < m_sparsePoints.size(); ++i) {
            alignedPoints[i] = {
                m_sparsePoints[i].x,
                m_sparsePoints[i].y,
                m_sparsePoints[i].value,
                0.0f  // padding
            };
        }
        
        wgpu::BufferDescriptor storageBufferDesc{};
        storageBufferDesc.size = alignedPoints.size() * sizeof(AlignedSparsePoint);
        storageBufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        m_storageBuffer = m_device.createBuffer(storageBufferDesc);
        m_queue.writeBuffer(m_storageBuffer, 0, alignedPoints.data(),
                            alignedPoints.size() * sizeof(AlignedSparsePoint));
        
        std::cout << "Storage buffer created with " << m_sparsePoints.size() << " points" << std::endl;
    }
    
    // 3. 创建uniform缓冲
    m_uniforms.gridWidth = static_cast<float>(m_header.width);
    m_uniforms.gridHeight = static_cast<float>(m_header.height);
    
    wgpu::BufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.size = sizeof(Uniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    m_uniformBuffer = m_device.createBuffer(uniformBufferDesc);
    
    UpdateUniforms(windowAspect);  // 初始化uniforms
    
    std::cout << "Uniform buffer created" << std::endl;
}

// 更新 Render 函数使用 bind group
void SparseDataVisualizer::Render(wgpu::RenderPassEncoder renderPass) {
    if (!m_pipeline || !m_bindGroup) {
        return;
    }
    
    renderPass.setPipeline(m_pipeline);
    renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);  // 设置 bind group
    renderPass.setVertexBuffer(0, m_vertexBuffer, 0, 16 * sizeof(float));  // 4个顶点，每个4个float
    renderPass.draw(4, 1, 0, 0);  // 4个顶点，三角形带
}