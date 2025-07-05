#pragma once
#include "ggl.h"
#include "PipelineManager.h"
#include "KDTreeWrapper.h"

class VolumeRenderingTest 
{
public:
    VolumeRenderingTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat);
    ~VolumeRenderingTest();

    struct RS_Uniforms_3D 
    {
        alignas(16) glm::mat4 viewMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 projMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 modelMatrix = glm::mat4(1.0f);
        alignas(16) glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
        float rayStepSize = 0.01f;
        alignas(16) glm::vec3 volumeSize = glm::vec3(1.0f);
        float volumeOpacity = 1.0f;
    };

    struct CS_Uniforms_3D 
    {
        float gridWidth = 128.0f;
        float gridHeight = 128.0f;
        float gridDepth = 128.0f;
        float padding1 = 0.0f;
    };

    // 确保结构体大小是16的倍数
    static_assert(sizeof(CS_Uniforms_3D) % 16 == 0, "CS_Uniforms_3D must be 16-byte aligned");
    static_assert(sizeof(RS_Uniforms_3D) % 16 == 0, "RS_Uniforms_3D must be 16-byte aligned");

    struct ComputeStage
    {
        wgpu::ComputePipeline pipeline = nullptr;
        wgpu::BindGroup bindGroup = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;

        bool Init(wgpu::Device device, wgpu::Queue queue, const CS_Uniforms_3D& uniforms);
        bool CreatePipeline(wgpu::Device device);
        bool UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputVolume3D);
        void RunCompute(wgpu::Device device, wgpu::Queue queue);
        void Release();
    private:
        bool InitUBO(wgpu::Device device, const CS_Uniforms_3D& uniforms);
    };

    struct RaycastingStage
    {
        wgpu::RenderPipeline pipeline = nullptr;
        wgpu::BindGroup bindGroup = nullptr;
        wgpu::Sampler volumeSampler = nullptr;
        wgpu::Sampler tfSampler = nullptr;
        wgpu::Buffer cubeVertexBuffer = nullptr;
        wgpu::Buffer cubeIndexBuffer = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        
        bool Init(wgpu::Device device, wgpu::Queue queue, const RS_Uniforms_3D& uniforms);
        bool CreatePipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat);
        bool InitBindGroup(wgpu::Device device, wgpu::TextureView volumeTexture, wgpu::TextureView transferFunction);
        void Render(wgpu::RenderPassEncoder renderPass);
        void Release();
        void UpdateUniforms(wgpu::Queue queue, const RS_Uniforms_3D& uniforms);
    private:
        bool InitCubeGeometry(wgpu::Device device, wgpu::Queue queue);
        bool InitSamplers(wgpu::Device device);
        bool InitUBO(wgpu::Device device, const RS_Uniforms_3D& uniforms);
    };

    bool Initialize(glm::mat4 vMat, glm::mat4 pMat);
    bool InitVolumeTexture(uint32_t width = 128, uint32_t height = 128, uint32_t depth = 128);
    void Render(wgpu::RenderPassEncoder renderPass);
    void UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix, glm::vec3 cameraPos);
    void UpdateTransferFunction(wgpu::TextureView tfTextureView);
    void SetRayStepSize(float stepSize);
    void SetVolumeOpacity(float opacity);

private:
    wgpu::Device m_device;
    wgpu::Queue m_queue;
    wgpu::TextureFormat m_swapChainFormat;
    
    RS_Uniforms_3D m_RS_Uniforms;
    CS_Uniforms_3D m_CS_Uniforms;
    
    wgpu::Texture m_volumeTexture;
    wgpu::TextureView m_volumeTextureView;
    wgpu::TextureView m_tfTextureView = nullptr;
    
    ComputeStage m_computeStage;
    RaycastingStage m_raycastingStage;
    
    bool m_needsUpdate = false;
};