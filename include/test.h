// TransferFunctionTest.h
#pragma once
#include "ggl.h"
#include <vector>
#include <webgpu/webgpu.hpp>


class TransferFunctionTest 
{


public:
    TransferFunctionTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat);
    ~TransferFunctionTest();

    struct SparsePoint 
    {
        float x;
        float y;
        float value;
        float padding;  // 填充到16字节对齐
    };

    struct RS_Uniforms 
    {
        alignas(16) glm::mat4 viewMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 projMatrix = glm::mat4(1.0f);
    };

    struct CS_Uniforms 
    {
        float minValue;
        float maxValue;
        float gridWidth;
        float gridHeight;
        uint32_t numPoints;
        float searchRadius;
        float padding[2];
    };

    struct DataHeader {
        uint32_t width;
        uint32_t height;
        uint32_t numPoints;
    };

    struct ComputeStage
    {
        wgpu::ComputePipeline pipeline = nullptr;
        wgpu::BindGroup data_bindGroup = nullptr;
        wgpu::BindGroup TF_bindGroup = nullptr;
        bool CreatePipeline(wgpu::Device device);
        bool UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputTexture);
        void RunCompute(wgpu::Device device, wgpu::Queue queue);
        void Release();
    };

    struct RenderStage
    {
        wgpu::RenderPipeline pipeline = nullptr;
        wgpu::BindGroup bindGroup = nullptr;
        wgpu::Sampler sampler = nullptr;
        wgpu::Buffer vertexBuffer = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        
        bool Init(wgpu::Device device, wgpu::Queue queue, RS_Uniforms uniforms);
        bool CreatePipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat);
        bool InitBindGroup(wgpu::Device device, wgpu::TextureView outputTexture);
        void Render(wgpu::RenderPassEncoder renderPass);
        void Release();
        void UpdateUniforms(wgpu::Queue queue, RS_Uniforms uniforms);
    private:
        bool InitVBO(wgpu::Device device, wgpu::Queue queue, float data_width, float data_height);
        bool InitUBO(wgpu::Device devic, RS_Uniforms uniforms);
        bool InitSampler(wgpu::Device device);
        bool InitResources(wgpu::Device device);
        
    };
    

    bool Initialize(glm::mat4 vMat, glm::mat4 pMat);
    bool InitSSBO();
    bool InitDataFromBinary(const std::string& filename);
    void Render(wgpu::RenderPassEncoder renderPass);
    void OnWindowResize(glm::mat4 veiwMatrix, glm::mat4 projMatrix);
    void UpdateSSBO(wgpu::TextureView tfTextureView);
    void ComputeValueRange();
    void UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix);
protected:
    std::vector<SparsePoint> m_sparsePoints;
    DataHeader m_header;
    RS_Uniforms m_RS_Uniforms;
    CS_Uniforms m_CS_Uniforms;
private:
    wgpu::Device m_device;
    wgpu::Queue m_queue;
    wgpu::TextureFormat m_swapChainFormat;
    wgpu::TextureView m_tfTextureView = nullptr;
    wgpu::Texture m_outputTexture;                     
    wgpu::TextureView m_outputTextureView;
    ComputeStage m_computeStage;
    RenderStage m_renderStage;
    bool m_needsUpdate = false;
};