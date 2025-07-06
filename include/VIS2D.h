#pragma once
#include "ggl.h"
#include "PipelineManager.h"
#include "KDTreeWrapper.h"



class VIS2D 
{

public:
    VIS2D(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat);
    ~VIS2D();

    
    struct RS_Uniforms 
    {
        alignas(16) glm::mat4 viewMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 projMatrix = glm::mat4(1.0f);
    };

    struct CS_Uniforms 
    {
        // 第一组：16字节对齐的float4
        float minValue;
        float maxValue;
        float gridWidth;
        float gridHeight;
        
        // 第二组：16字节对齐的uint4
        uint32_t totalNodes;
        uint32_t totalPoints;
        uint32_t numLevels;
        uint32_t interpolationMethod;
        
        // 第三组：16字节对齐，包含searchRadius和padding
        float searchRadius;
        float padding1;
        float padding2;
        float padding3;
    };

    // 确保结构体大小是16的倍数
    static_assert(sizeof(CS_Uniforms) % 16 == 0, "CS_Uniforms must be 16-byte aligned");
    static_assert(sizeof(CS_Uniforms) == 48, "CS_Uniforms should be exactly 48 bytes");

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
        wgpu::BindGroup KDTree_bindGroup = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        wgpu::Buffer storageBuffer = nullptr;
        wgpu::Buffer kdNodesBuffer = nullptr;

        bool Init(wgpu::Device device, wgpu::Queue queue, 
            const std::vector<SparsePoint>& sparsePoints, 
            const KDTreeBuilder::TreeData& kdTreeData,
            const CS_Uniforms uniforms);
        bool CreatePipeline(wgpu::Device device);
        bool UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputTexture);
        void RunCompute(wgpu::Device device, wgpu::Queue queue);
        void Release();
    private:
        bool InitKDTreeBuffers(wgpu::Device device, wgpu::Queue queue, 
            const KDTreeBuilder::TreeData& kdTreeData);
        bool InitSSBO(wgpu::Device device, wgpu::Queue queue, const std::vector<SparsePoint>& sparsePoints);
        bool InitUBO(wgpu::Device device, CS_Uniforms uniforms);
    };

    struct RenderStage
    {
        wgpu::RenderPipeline pipeline = nullptr;
        wgpu::BindGroup bindGroup = nullptr;
        wgpu::Sampler sampler = nullptr;
        wgpu::Buffer vertexBuffer = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        
        bool Init(wgpu::Device device, wgpu::Queue queue, RS_Uniforms uniforms, float data_width, float data_height);
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
    bool InitOutputTexture(uint32_t width = 512, uint32_t height = 512, uint32_t depth = 1, wgpu::TextureFormat format = wgpu::TextureFormat::RGBA16Float);
    bool InitDataFromBinary(const std::string& filename);
    void Render(wgpu::RenderPassEncoder renderPass);
    void OnWindowResize(glm::mat4 veiwMatrix, glm::mat4 projMatrix);
    void UpdateSSBO(wgpu::TextureView tfTextureView);
    void ComputeValueRange();
    void UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix);
    void SetInterpolationMethod(int kValue);
    void SetSearchRadius(float radius);
protected:
    std::vector<SparsePoint> m_sparsePoints;
    DataHeader m_header;
    RS_Uniforms m_RS_Uniforms;
    CS_Uniforms m_CS_Uniforms;
    KDTreeBuilder::TreeData m_KDTreeData;
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