#pragma once
#include <webgpu/webgpu.hpp>
#include <webgpu/webgpu.h>  // 添加 C API 头文件
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>

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
    SparseDataVisualizer(wgpu::Device device, wgpu::Queue queue);
    ~SparseDataVisualizer();

    // 从二进制文件加载数据
    bool LoadFromBinary(const std::string& filename);
    
    // 创建GPU资源
    void CreateBuffers(int widowWidth, int windowHeight);
    void CreatePipeline(wgpu::TextureFormat swapChainFormat);
    
    // 渲染
    void Render(wgpu::RenderPassEncoder renderPass);
    
    // 更新视图矩阵等
    void UpdateUniforms(float aspectRatio);
    void OnWindowResize(int width, int height);

private:
    wgpu::Device m_device;
    wgpu::Queue m_queue;
    
    int m_windowWidth = 0;
    int m_windowHeight = 0;

    // 数据
    std::vector<SparsePoint> m_sparsePoints;
    DataHeader m_header;
    
    // GPU资源
    wgpu::Buffer m_vertexBuffer = nullptr;
    wgpu::Buffer m_uniformBuffer = nullptr;
    wgpu::Buffer m_storageBuffer = nullptr;  // 存储稀疏点数据
    wgpu::BindGroup m_bindGroup = nullptr;
    wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
    wgpu::RenderPipeline m_pipeline = nullptr;
    
    // 统一变量
    struct Uniforms {
        float viewMatrix[16];
        float projMatrix[16];
        float gridWidth;
        float gridHeight;
        float minValue;
        float maxValue;
    };
    Uniforms m_uniforms;
    
    // 辅助函数
    void CreateFullscreenQuad();
    float FindNearestValue(float x, float y);
    void ComputeValueRange();
};