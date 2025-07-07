#pragma once
#include "ggl.h"
#include "KDTreeWrapper.h"

class VIS3D 
{

public:
    VIS3D(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat);
    ~VIS3D();

    
    struct RS_Uniforms 
    {
        alignas(16) glm::mat4 viewMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 projMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 modelMatrix = glm::mat4(1.0f);
    };

    struct CS_Uniforms 
    {
        float minValue;
        float maxValue;
        float gridWidth;
        float gridHeight;

        float gridDepth;
        float searchRadius;
        float padding1;
        float padding2;

        uint32_t totalNodes;
        uint32_t totalPoints;
        uint32_t numLevels;
        uint32_t interpolationMethod;

    };

    // 确保结构体大小是16的倍数
    static_assert(sizeof(VIS3D::CS_Uniforms) % 16 == 0, "CS_Uniforms must be 16-byte aligned");
    static_assert(sizeof(VIS3D::CS_Uniforms) == 48, "CS_Uniforms should be exactly 64 bytes");

    struct DataHeader {
        uint32_t width;
        uint32_t height;
        uint32_t depth; 
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
            const std::vector<SparsePoint3D>& sparsePoints, 
            const KDTreeBuilder3D::TreeData3D& kdTreeData,
            const CS_Uniforms uniforms);
        bool CreatePipeline(wgpu::Device device);
        bool UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputTexture);
        void RunCompute(wgpu::Device device, wgpu::Queue queue);
        void Release();
    private:
        bool InitKDTreeBuffers(wgpu::Device device, wgpu::Queue queue, 
            const KDTreeBuilder3D::TreeData3D& kdTreeData);
        bool InitSSBO(wgpu::Device device, wgpu::Queue queue, const std::vector<SparsePoint3D>& sparsePoints);
        bool InitUBO(wgpu::Device device, CS_Uniforms uniforms);
    };

    struct RenderStage
    {
        wgpu::RenderPipeline pipeline = nullptr;
        wgpu::BindGroup bindGroup = nullptr;
        wgpu::Sampler sampler = nullptr;
        wgpu::Buffer vertexBuffer = nullptr;
        wgpu::Buffer indexBuffer = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        uint32_t indexCount = 0;
        
        bool Init(wgpu::Device device, wgpu::Queue queue, RS_Uniforms uniforms, float data_width, float data_height, float data_depth);
        bool CreatePipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat);
        bool InitBindGroup(wgpu::Device device, wgpu::TextureView outputTexture);
        void Render(wgpu::RenderPassEncoder renderPass);
        void Release();
        void UpdateUniforms(wgpu::Queue queue, RS_Uniforms uniforms);
    private:
        bool InitVBO(wgpu::Device device, wgpu::Queue queue, float data_width, float data_height, float data_depth);
        bool InitEBO(wgpu::Device device, wgpu::Queue queue);
        bool InitUBO(wgpu::Device devic, RS_Uniforms uniforms);
        bool InitSampler(wgpu::Device device);
        bool InitResources(wgpu::Device device);
        
    };
    

    bool Initialize(glm::mat4 vMat, glm::mat4 pMat);
    bool InitOutputTexture(uint32_t width = 128, uint32_t height = 128, uint32_t depth = 128, wgpu::TextureFormat format = wgpu::TextureFormat::RGBA16Float);
    bool InitDataFromBinary(const std::string& filename);
    bool InitDataFromBinary(const std::string& path, uint32_t w=64, uint32_t h=64, uint32_t d=64);
    void Render(wgpu::RenderPassEncoder renderPass);
    void OnWindowResize(glm::mat4 veiwMatrix, glm::mat4 projMatrix);
    void UpdateSSBO(wgpu::TextureView tfTextureView);
    void ComputeValueRange();
    void UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix);
    void SetInterpolationMethod(int kValue);
    void SetSearchRadius(float radius);
    void SetModelMatrix(glm::mat4 modelMatrix);
protected:
    std::vector<SparsePoint3D> m_sparsePoints;
    DataHeader m_header;
    RS_Uniforms m_RS_Uniforms;
    CS_Uniforms m_CS_Uniforms;
    KDTreeBuilder3D::TreeData3D m_KDTreeData;
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