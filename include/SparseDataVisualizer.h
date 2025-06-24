#pragma once
#include <webgpu/webgpu.hpp>
#include "WGSLShaderProgram.h"
#include "Camera.hpp"


struct SparsePoint {
    float x;
    float y;
    float value;
    float padding;  // 填充到16字节对齐
};

struct DataHeader {
    uint32_t width;
    uint32_t height;
    uint32_t numPoints;
};

class SparseDataVisualizer {
public:
    SparseDataVisualizer(wgpu::Device device, wgpu::Queue queue, Camera* camera = new Camera(Camera::CameraMode::Ortho2D));
    virtual ~SparseDataVisualizer();

    // 从二进制文件加载数据
    bool LoadFromBinary(const std::string& filename);
    
    // 创建GPU资源
    void CreateBuffers(int widowWidth, int windowHeight);
    virtual void CreatePipeline(wgpu::TextureFormat swapChainFormat);
    
    // 渲染
    virtual void Render(wgpu::RenderPassEncoder renderPass);
    
    // 更新视图矩阵等
    void UpdateUniforms(float aspectRatio);
    virtual void OnWindowResize(int width, int height);

    // 设置 Camera
    void SetCamera(Camera* camera) { m_camera = camera; }

protected:
    wgpu::Device m_device;
    wgpu::Queue m_queue;
    
    int m_windowWidth = 0;
    int m_windowHeight = 0;

    Camera* m_camera = nullptr; // 摄像机对象，用于视图变换

    // 数据
    std::vector<SparsePoint> m_sparsePoints;
    DataHeader m_header;
    
    // GPU资源
    std::unique_ptr<WGSLShaderProgram> m_shaderProgram;
    wgpu::Buffer m_vertexBuffer = nullptr;
    wgpu::Buffer m_uniformBuffer = nullptr;
    wgpu::Buffer m_storageBuffer = nullptr;  // 存储稀疏点数据
    wgpu::BindGroup m_bindGroup = nullptr;
    wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
    wgpu::RenderPipeline m_pipeline = nullptr;
    
    // 统一变量
    struct Uniforms 
    {
        alignas(16) glm::mat4 viewMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 projMatrix = glm::mat4(1.0f);
        alignas(4)  float gridWidth;
        alignas(4)  float gridHeight;
        alignas(4)  float minValue;
        alignas(4)  float maxValue;
    };
    Uniforms m_uniforms;
    
    // 辅助函数
    void CreateVBO(int windowWidth, int windowHeight);
    void CreateSSBO();
    void CreateUBO(float aspectRatio);
    void CreateBindGroupLayout();
    void CreateBindGroup();
    wgpu::VertexBufferLayout CreateVertexLayout();
    void ComputeValueRange();
};