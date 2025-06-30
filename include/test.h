// TransferFunctionTest.h
#pragma once
#include <webgpu/webgpu.hpp>
#include <vector>

class TransferFunctionTest {
public:
    TransferFunctionTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat);
    ~TransferFunctionTest();
    
    bool Initialize();
    void Render(wgpu::RenderPassEncoder renderPass);
    void OnKeyPress(int key, int action);
    void OnWindowResize(int width, int height);
    void SetExternalTransferFunction(wgpu::TextureView tfTextureView);
    
private:
    // WebGPU resources
    wgpu::Device m_device;
    wgpu::Queue m_queue;
    wgpu::TextureFormat m_swapChainFormat;
    
    // 输入：不同的传输函数纹理
    std::vector<wgpu::Texture> m_inputTFTextures;      // 存储不同的TF图像
    std::vector<wgpu::TextureView> m_inputTFViews;     // 对应的TextureView
    int m_currentTFIndex = 0;
    
    wgpu::TextureView m_externalTFView;
    bool m_useExternalTF = false; // 是否使用外部传输函数纹理

    // 计算着色器资源
    wgpu::ComputePipeline m_computePipeline;
    std::vector<wgpu::BindGroup> m_computeBindGroups;  // 每个TF对应一个BindGroup
    wgpu::BindGroup m_externalBindGroup = nullptr; // 外部传输函数的BindGroup

    // 输出：CS生成的结果纹理
    wgpu::Texture m_outputTexture;                     // CS的输出纹理
    wgpu::TextureView m_outputTextureView;
    
    // 渲染管线资源  
    wgpu::RenderPipeline m_renderPipeline;
    wgpu::BindGroup m_renderBindGroup;
    wgpu::Buffer m_vertexBuffer;
    wgpu::Sampler m_renderSampler;                     // 用于渲染时采样输出纹理
    
    // Helper methods
    bool CreateInputTFTextures();
    bool CreateComputePipeline();
    bool CreateRenderPipeline();
    bool CreateResources();
    void RunComputeShader();
    void SwitchTransferFunction();
    void UpdateExternalBindGroup();  // 新增：更新外部TF绑定组
};







