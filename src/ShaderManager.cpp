#include "ShaderManager.h"


ShaderManager& ShaderManager::getInstance() 
{
    static ShaderManager instance;
    return instance;
}


std::string ShaderManager::loadFile(const std::string& filePath) {
    auto it = m_cache.find(filePath);
    if (it != m_cache.end()) return it->second;
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Cannot open: " << filePath << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    m_cache[filePath] = content;
    
    std::cout << "[ShaderManager] Loaded: " << filePath << std::endl;
    return content;
}

wgpu::ShaderModule ShaderManager::loadShader(wgpu::Device device, const std::string& shaderPath) {
    std::string source = loadFile(shaderPath);
    if (source.empty()) return nullptr;
    
    return createFromSource(device, source, shaderPath);
}

wgpu::ShaderModule ShaderManager::createFromSource(wgpu::Device device, const std::string& source, const std::string& label) {
    wgpu::ShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgslDesc.code = source.c_str();

    wgpu::ShaderModuleDescriptor desc = {};
    desc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct*>(&wgslDesc);
    desc.label = label.c_str();
    
    return device.createShaderModule(desc);
}

void ShaderManager::clearCache() {
    m_cache.clear();
}