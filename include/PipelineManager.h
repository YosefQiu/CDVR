#pragma once
#include "ggl.h"
#include "ShaderManager.h"

struct BlendPresets {
    static wgpu::BlendState alphaBlending() {
        wgpu::BlendState blend = {};
        blend.color.operation = wgpu::BlendOperation::Add;
        blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blend.alpha.operation = wgpu::BlendOperation::Add;
        blend.alpha.srcFactor = wgpu::BlendFactor::One;
        blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        return blend;
    }
    
    static wgpu::BlendState additiveBlending() {
        wgpu::BlendState blend = {};
        blend.color.operation = wgpu::BlendOperation::Add;
        blend.color.srcFactor = wgpu::BlendFactor::One;
        blend.color.dstFactor = wgpu::BlendFactor::One;
        blend.alpha.operation = wgpu::BlendOperation::Add;
        blend.alpha.srcFactor = wgpu::BlendFactor::One;
        blend.alpha.dstFactor = wgpu::BlendFactor::One;
        return blend;
    }
    
    static wgpu::BlendState premultipliedAlpha() {
        wgpu::BlendState blend = {};
        blend.color.operation = wgpu::BlendOperation::Add;
        blend.color.srcFactor = wgpu::BlendFactor::One;
        blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blend.alpha.operation = wgpu::BlendOperation::Add;
        blend.alpha.srcFactor = wgpu::BlendFactor::One;
        blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        return blend;
    }
};

// 预定义的深度状态
struct DepthPresets {
    static wgpu::DepthStencilState noDepth() {
        // 返回默认构造的结构体，相当于禁用深度
        return {};
    }
    
    static wgpu::DepthStencilState readOnlyDepth(wgpu::TextureFormat depthFormat = wgpu::TextureFormat::Depth24Plus) {
        wgpu::StencilFaceState stencil = {};
        stencil.compare = wgpu::CompareFunction::Always;
        stencil.failOp = wgpu::StencilOperation::Keep;
        stencil.depthFailOp = wgpu::StencilOperation::Keep;
        stencil.passOp = wgpu::StencilOperation::Keep;
        
        wgpu::DepthStencilState depth = {};
        depth.format = depthFormat;
        depth.depthWriteEnabled = false;  // 只读
        depth.depthCompare = wgpu::CompareFunction::Always;
        depth.stencilFront = stencil;
        depth.stencilBack = stencil;
        depth.stencilReadMask = 0xFFFFFFFF;
        depth.stencilWriteMask = 0xFFFFFFFF;
        return depth;
    }
    
    static wgpu::DepthStencilState standardDepth(wgpu::TextureFormat depthFormat = wgpu::TextureFormat::Depth24Plus) {
        wgpu::StencilFaceState stencil = {};
        stencil.compare = wgpu::CompareFunction::Always;
        stencil.failOp = wgpu::StencilOperation::Keep;
        stencil.depthFailOp = wgpu::StencilOperation::Keep;
        stencil.passOp = wgpu::StencilOperation::Keep;
        
        wgpu::DepthStencilState depth = {};
        depth.format = depthFormat;
        depth.depthWriteEnabled = true;
        depth.depthCompare = wgpu::CompareFunction::Less;
        depth.stencilFront = stencil;
        depth.stencilBack = stencil;
        depth.stencilReadMask = 0xFFFFFFFF;
        depth.stencilWriteMask = 0xFFFFFFFF;
        return depth;
    }
};

struct VertexLayoutBuilder {
    static wgpu::VertexBufferLayout createPositionTexCoord() {
        static wgpu::VertexAttribute attrs[2];
        attrs[0].offset = 0;
        attrs[0].shaderLocation = 0;
        attrs[0].format = wgpu::VertexFormat::Float32x2;
        
        attrs[1].offset = 2 * sizeof(float);
        attrs[1].shaderLocation = 1;
        attrs[1].format = wgpu::VertexFormat::Float32x2;
        
        static wgpu::VertexBufferLayout layout = {};
        layout.arrayStride = 4 * sizeof(float);
        layout.stepMode = wgpu::VertexStepMode::Vertex;
        layout.attributeCount = 2;
        layout.attributes = attrs;
        return layout;
    }
    
    static wgpu::VertexBufferLayout createPositionColorTexCoord() {
        static wgpu::VertexAttribute attrs[3];
        attrs[0].offset = 0;
        attrs[0].shaderLocation = 0;
        attrs[0].format = wgpu::VertexFormat::Float32x3;  // position
        
        attrs[1].offset = 3 * sizeof(float);
        attrs[1].shaderLocation = 1;
        attrs[1].format = wgpu::VertexFormat::Float32x4;  // color
        
        attrs[2].offset = 7 * sizeof(float);
        attrs[2].shaderLocation = 2;
        attrs[2].format = wgpu::VertexFormat::Float32x2;  // texCoord
        
        static wgpu::VertexBufferLayout layout = {};
        layout.arrayStride = 9 * sizeof(float);
        layout.stepMode = wgpu::VertexStepMode::Vertex;
        layout.attributeCount = 3;
        layout.attributes = attrs;
        return layout;
    }
};

// 渲染管线构建器类
class RenderPipelineBuilder {
public:
    RenderPipelineBuilder& setDevice(wgpu::Device device);
    RenderPipelineBuilder& setLabel(const std::string& label);
    RenderPipelineBuilder& setVertexEntry(const std::string& entry);
    RenderPipelineBuilder& setFragmentEntry(const std::string& entry);
    // 着色器设置
    RenderPipelineBuilder& setVertexShader(const std::string& path, const std::string& entry = "main");
    RenderPipelineBuilder& setFragmentShader(const std::string& path, const std::string& entry = "main");
    RenderPipelineBuilder& setVertexShaderSource(const std::string& source, const std::string& entry = "main");
    RenderPipelineBuilder& setFragmentShaderSource(const std::string& source, const std::string& entry = "main");
    
    // 顶点布局
    RenderPipelineBuilder& setVertexLayout(const wgpu::VertexBufferLayout& layout);
    
    // 渲染状态
    RenderPipelineBuilder& setSwapChainFormat(wgpu::TextureFormat format);
    RenderPipelineBuilder& setPrimitiveTopology(wgpu::PrimitiveTopology topology);
    RenderPipelineBuilder& setCullMode(wgpu::CullMode cullMode);
    RenderPipelineBuilder& setFrontFace(wgpu::FrontFace frontFace);
    
    // 混合状态
    RenderPipelineBuilder& setBlendState(const wgpu::BlendState& blendState);
    RenderPipelineBuilder& setAlphaBlending();  // 快捷方法
    RenderPipelineBuilder& setAdditiveBlending();  // 快捷方法
    RenderPipelineBuilder& setPremultipliedAlpha();  // 快捷方法
    RenderPipelineBuilder& disableBlending();
    
    // 深度状态
    RenderPipelineBuilder& setDepthStencilState(const wgpu::DepthStencilState& depthState);
    RenderPipelineBuilder& setReadOnlyDepth(wgpu::TextureFormat format = wgpu::TextureFormat::Depth24Plus);  // 快捷方法
    RenderPipelineBuilder& setStandardDepth(wgpu::TextureFormat format = wgpu::TextureFormat::Depth24Plus);  // 快捷方法
    RenderPipelineBuilder& disableDepth();
    
    // 多重采样
    RenderPipelineBuilder& setMultisample(uint32_t count, uint32_t mask = 0xFFFFFFFF, bool alphaToCoverage = false);
    
    // 构建管线
    wgpu::RenderPipeline build();

private:
    wgpu::Device m_device;
    std::string m_label = "Render Pipeline";
    
    std::string m_vertexShaderPath;
    std::string m_fragmentShaderPath;
    std::string m_vertexShaderSource;
    std::string m_fragmentShaderSource;
    std::string m_vertexEntry = "main";
    std::string m_fragmentEntry = "main";
    
    std::optional<wgpu::VertexBufferLayout> m_vertexLayout;
    wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::BGRA8Unorm;
    wgpu::PrimitiveTopology m_topology = wgpu::PrimitiveTopology::TriangleStrip;
    wgpu::CullMode m_cullMode = wgpu::CullMode::None;
    wgpu::FrontFace m_frontFace = wgpu::FrontFace::CCW;
    
    std::optional<wgpu::BlendState> m_blendState;
    std::optional<wgpu::DepthStencilState> m_depthState;
    
    uint32_t m_multisampleCount = 1;
    uint32_t m_multisampleMask = 0xFFFFFFFF;
    bool m_alphaToCoverage = false;
    
    bool m_useVertexShaderPath = true;
    bool m_useFragmentShaderPath = true;
};

// 计算管线构建器类（相对简单）
class ComputePipelineBuilder {
public:
    ComputePipelineBuilder& setDevice(wgpu::Device device);
    ComputePipelineBuilder& setLabel(const std::string& label);
    ComputePipelineBuilder& setShader(const std::string& path, const std::string& entry = "main");
    ComputePipelineBuilder& setShaderSource(const std::string& source, const std::string& entry = "main");
    ComputePipelineBuilder& setEntry(const std::string& entry);
    wgpu::ComputePipeline build();

private:
    wgpu::Device m_device;
    std::string m_label = "Compute Pipeline";
    std::string m_shaderPath;
    std::string m_shaderSource;
    std::string m_entry = "main";
    bool m_useShaderPath = true;
};

class PipelineManager {
public:
    static PipelineManager& getInstance();
    
    // 返回构建器，允许链式调用
    RenderPipelineBuilder createRenderPipeline();
    ComputePipelineBuilder createComputePipeline();

private:
    PipelineManager() = default;
};