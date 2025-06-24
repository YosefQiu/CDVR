#pragma once
#include "ggl.h"
#include <webgpu/webgpu.hpp>

class WGSLShaderProgram 
{
public:
    WGSLShaderProgram(wgpu::Device device);
    bool LoadShaders(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
    void CreatePipeline(wgpu::TextureFormat swapChainFormat,
                                       const wgpu::BindGroupLayout& bindGroupLayout,
                                       const wgpu::VertexBufferLayout& vertexLayout);
    wgpu::ShaderModule GetVertexShader() const { return m_vertexShader; }
    wgpu::ShaderModule GetFragmentShader() const { return m_fragmentShader; }
    void SetVertexShader(wgpu::ShaderModule shader) { m_vertexShader = shader; }
    void SetFragmentShader(wgpu::ShaderModule shader) { m_fragmentShader = shader; }
    wgpu::RenderPipeline GetPipeline() const { return m_pipeline; }

private:
    wgpu::Device m_device;
    wgpu::ShaderModule m_vertexShader;
    wgpu::ShaderModule m_fragmentShader;    
    wgpu::RenderPipeline m_pipeline;
};