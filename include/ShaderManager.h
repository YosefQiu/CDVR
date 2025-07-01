#pragma once
#include "ggl.h"
#include <webgpu/webgpu.hpp>


class ShaderManager 
{
public:
    static ShaderManager& getInstance();
    
    wgpu::ShaderModule loadShader(wgpu::Device device, const std::string& shaderPath);
    wgpu::ShaderModule createFromSource(wgpu::Device device, const std::string& source, const std::string& label);
    void clearCache();

private:
    ShaderManager() = default;
    std::string loadFile(const std::string& filePath);
    std::unordered_map<std::string, std::string> m_cache;
};