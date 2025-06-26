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
    
    bool LoadComputeShader(const std::string& computeShaderPath);
    void CreateComputePipeline(const wgpu::BindGroupLayout& bindGroupLayout1, const wgpu::BindGroupLayout& layout2);
    
    wgpu::ShaderModule GetVertexShader() const { return m_vertexShader; }
    wgpu::ShaderModule GetFragmentShader() const { return m_fragmentShader; }
    wgpu::ShaderModule GetComputeShader() const { return m_computeShader; }
    wgpu::RenderPipeline GetPipeline() const { return m_pipeline; }
    wgpu::ComputePipeline GetComputePipeline() const { return m_computePipeline; }
    
    void SetVertexShader(wgpu::ShaderModule shader) { m_vertexShader = shader; }
    void SetFragmentShader(wgpu::ShaderModule shader) { m_fragmentShader = shader; }
    void SetComputeShader(wgpu::ShaderModule shader) { m_computeShader = shader; }
    
    

private:
    wgpu::Device m_device;
    wgpu::ShaderModule m_vertexShader;
    wgpu::ShaderModule m_fragmentShader;    
    wgpu::ShaderModule m_computeShader;
    wgpu::RenderPipeline m_pipeline;
    wgpu::ComputePipeline m_computePipeline;
};