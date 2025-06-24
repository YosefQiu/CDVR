#include "SparseDataVisualizer.h"
#include <webgpu/webgpu.hpp>


SparseDataVisualizer::SparseDataVisualizer(wgpu::Device device, wgpu::Queue queue, 
                                           Camera* camera)
    : m_device(device), m_queue(queue), m_camera(camera) {
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


void SparseDataVisualizer::UpdateUniforms([[maybe_unused]]float aspectRatio) 
{
    // 使用相机系统
    m_camera->SetViewportSize(m_windowWidth, m_windowHeight);
    
    // 从相机获取矩阵
    glm::mat4 view = m_camera->GetViewMatrix();
    glm::mat4 proj = m_camera->GetProjMatrix();
    
    // 转置矩阵（GLM是列主序，WGSL是行主序）
    m_uniforms.viewMatrix = (view);
    m_uniforms.projMatrix = (proj);
    
    m_uniforms.gridWidth = static_cast<float>(m_header.width);
    m_uniforms.gridHeight = static_cast<float>(m_header.height);
    
    m_queue.writeBuffer(m_uniformBuffer, 0, &m_uniforms, sizeof(Uniforms));
}

void SparseDataVisualizer::OnWindowResize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    
    // 保持相机视图不变，只更新视口大小
    m_camera->SetViewportSize(width, height);
    
    // 更新uniforms
    UpdateUniforms(static_cast<float>(width) / static_cast<float>(height));
}

void SparseDataVisualizer::CreateVBO(int windowWidth, int windowHeight)
{
    // 计算正确的宽高比
    float dataAspect = static_cast<float>(m_header.width) / static_cast<float>(m_header.height);  // 150/450 = 0.333
    float windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);  // 约 1.68
    float data_width = static_cast<float>(m_header.width);    // 150
    float data_height = static_cast<float>(m_header.height);  // 450
    
    
    
    // 1. 创建保持宽高比的四边形顶点
    // 使用数据空间坐标
    float vertices[] = {
        // posX, posY,              u,    v
        0.0f,        0.0f,         0.0f, 0.0f,  // 左下
        data_width,  0.0f,         1.0f, 0.0f,  // 右下
        0.0f,        data_height,  0.0f, 1.0f,  // 左上
        data_width,  data_height,  1.0f, 1.0f,  // 右上
    };
    
    wgpu::BufferDescriptor vertexBufferDesc{};
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    vertexBufferDesc.mappedAtCreation = false;
    m_vertexBuffer = m_device.createBuffer(vertexBufferDesc);
    m_queue.writeBuffer(m_vertexBuffer, 0, vertices, sizeof(vertices));
    
    std::cout << "Vertex buffer created with aspect correction" << std::endl;
    std::cout << "Data aspect: " << dataAspect << ", Window aspect: " << windowAspect << std::endl;
   

}

void SparseDataVisualizer::CreateSSBO()
{
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
}

void SparseDataVisualizer::CreateUBO(float aspectRatio)
{
    m_uniforms.gridWidth = static_cast<float>(m_header.width);
    m_uniforms.gridHeight = static_cast<float>(m_header.height);
    
    wgpu::BufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.size = sizeof(Uniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    m_uniformBuffer = m_device.createBuffer(uniformBufferDesc);
    
    UpdateUniforms(aspectRatio);  // 初始化 uniform 数据

    std::cout << "Uniform buffer created" << std::endl;
}

void SparseDataVisualizer::CreateBindGroupLayout()
{
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
}

void SparseDataVisualizer::CreateBindGroup()
{
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
}

wgpu::VertexBufferLayout SparseDataVisualizer::CreateVertexLayout()
{
    static wgpu::VertexAttribute vertexAttributes[2] = {};
    vertexAttributes[0].format = wgpu::VertexFormat::Float32x2;
    vertexAttributes[0].offset = 0;
    vertexAttributes[0].shaderLocation = 0;

    vertexAttributes[1].format = wgpu::VertexFormat::Float32x2;
    vertexAttributes[1].offset = 2 * sizeof(float);
    vertexAttributes[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = 4 * sizeof(float);
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = vertexAttributes;

    return vertexBufferLayout;
}

void SparseDataVisualizer::CreatePipeline(wgpu::TextureFormat swapChainFormat) {

    // 创建 bind group layout
    CreateBindGroupLayout();
    // 创建 bind group
    CreateBindGroup();
    
    // 创建顶点布局（位置 + 纹理坐标）
   wgpu::VertexBufferLayout vertexBufferLayout = CreateVertexLayout();
    
    // 创建着色器程序
    m_shaderProgram = std::make_unique<WGSLShaderProgram>(m_device);
    bool success = m_shaderProgram->LoadShaders("../shaders/sparse_data.vert.wgsl",
                                                "../shaders/sparse_data.frag.wgsl");
   if (!success) 
   {
       std::cerr << "Failed to load shaders!" << std::endl;
       return;
   }
   m_shaderProgram->CreatePipeline(swapChainFormat, m_bindGroupLayout, vertexBufferLayout);
   m_pipeline = m_shaderProgram->GetPipeline();
}

// CreateBuffers 
void SparseDataVisualizer::CreateBuffers(int windowWidth, int windowHeight) 
{

    // 顶点缓冲 VBO
    // 存储缓冲 SSBO
    // uniform缓冲 UBO

    m_windowWidth = windowWidth;
    m_windowHeight = windowHeight;

    // 1. 创建顶点缓冲，保持宽高比
    CreateVBO(m_windowWidth, m_windowHeight);
    // 2. 创建存储缓冲，存放稀疏点数据
    CreateSSBO();
    // 3. 创建uniform缓冲
    CreateUBO(static_cast<float>(windowWidth) / static_cast<float>(windowHeight));
}

// 更新 Render 函数使用 bind group
void SparseDataVisualizer::Render(wgpu::RenderPassEncoder renderPass) {
    if (!m_pipeline || !m_bindGroup) {
        return;
    }
    
    renderPass.setPipeline(m_pipeline);
    renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);  // 设置 bind group
    renderPass.setVertexBuffer(0, m_vertexBuffer, 0, 4 * 4 * sizeof(float));  // 4个顶点，每个4个float
    renderPass.draw(4, 1, 0, 0);  // 4个顶点，三角形带
} 