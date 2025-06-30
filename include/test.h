// TransferFunctionTest.h
#pragma once
#include "ggl.h"


class TransferFunctionTest {
public:
    TransferFunctionTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat);
    ~TransferFunctionTest();
    
    bool Initialize();
    void Render(wgpu::RenderPassEncoder renderPass);
    void RenderWithTF(wgpu::RenderPassEncoder renderPass, wgpu::TextureView tfTextureView);
    void OnWindowResize(int width, int height);
    void SetExternalTransferFunction(wgpu::TextureView tfTextureView);
    void UpdateIfNeeded();
    void RunComputeShader();
    bool CheckAndPrepareUpdate(wgpu::TextureView tfTextureView); 
private:
    // WebGPU resources
    wgpu::Device m_device;
    wgpu::Queue m_queue;
    wgpu::TextureFormat m_swapChainFormat;
    bool m_needsUpdate = false;
    wgpu::TextureView m_lastTFView = nullptr;
    wgpu::TextureView m_tfTextureView = nullptr;

    // 计算着色器资源
    wgpu::ComputePipeline m_computePipeline;
    wgpu::BindGroup m_computeBindGroup = nullptr;

    // 输出：CS生成的结果纹理
    wgpu::Texture m_outputTexture;                     // CS的输出纹理
    wgpu::TextureView m_outputTextureView;
    
    // 渲染管线资源  
    wgpu::RenderPipeline m_renderPipeline;
    wgpu::BindGroup m_renderBindGroup;
    wgpu::Buffer m_vertexBuffer;
    wgpu::Sampler m_renderSampler;                     // 用于渲染时采样输出纹理
    
    // Helper methods
    bool CreateComputePipeline();
    bool CreateRenderPipeline();
    bool CreateResources();
    
    void UpdateComputeBindGroup();
};







