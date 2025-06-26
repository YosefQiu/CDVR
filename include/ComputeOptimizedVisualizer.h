#pragma once
#include "ggl.h"
#include "SparseDataVisualizer.h"   
#include <webgpu/webgpu.hpp>


class ComputeOptimizedVisualizer : public SparseDataVisualizer 
{
public:
    ComputeOptimizedVisualizer(wgpu::Device device, wgpu::Queue queue, Camera* camera = new Camera(Camera::CameraMode::Ortho2D))
        : SparseDataVisualizer(device, queue, camera) {}

    // 重载创建管线方法，使用计算着色器优化渲染
    void CreatePipeline(wgpu::TextureFormat swapChainFormat) override;
    void Render(wgpu::RenderPassEncoder renderPass) override;
    void OnWindowResize(int width, int height) override;

    void UpdateDataTexture();
    void UpdateComputeUniforms();
    void UpdateTransferFunction(wgpu::TextureView tfTextureView, wgpu::Sampler tfSampler);
private:
    // compute shader 相关
    wgpu::ComputePipeline m_computePipeline;
    wgpu::BindGroup m_computeBindGroup;
    wgpu::BindGroupLayout m_computeBindGroupLayout;

    wgpu::BindGroupLayout m_transferFunctionBindGroupLayout;
    wgpu::BindGroup m_transferFunctionBindGroup;
    wgpu::PipelineLayout m_computePipelineLayout;

    // 数据纹理相关
    wgpu::Texture m_dataTexture;
    wgpu::TextureView m_dataTextureView;
    wgpu::Sampler m_dataSampler;

    // Transfer Function 相关
    wgpu::Texture m_DefaultTFTexture;
    wgpu::TextureView m_DefaultTFTextureView;

    // Compute Uniforms
    struct ComputeUniforms {
        float minValue;
        float maxValue;
        float gridWidth;
        float gridHeight;
        uint32_t numPoints;
        float searchRadius;
        float padding[2];
    };

    wgpu::Buffer m_computeUniformBuffer;
    ComputeUniforms m_computeUniforms;

    std::unique_ptr<WGSLShaderProgram> m_computeShaderProgram;
    std::unique_ptr<WGSLShaderProgram> m_renderShaderProgram;

    wgpu::RenderPipeline m_simplifiedRenderPipeline; // 渲染管线
    wgpu::BindGroupLayout m_renderBindGroupLayout; // 渲染绑定组
    wgpu::BindGroup m_renderBindGroup; // 渲染绑定组实例

    void CreateComputeResources();
    void CreateDataTexture();
    void CreateSimplifiedRenderPipeline(wgpu::TextureFormat format);
    

};