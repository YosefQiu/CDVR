#pragma once
#include "ggl.h"
#include "SparseDataVisualizer.h"
#include <webgpu/webgpu.hpp>
#include <memory>
struct GLFWwindow;

class Application
{
public:
    bool Initialize(int width = 640, int height = 480, const char* title = "Learn WebGPU");
    void Terminate();
    void MainLoop();
    bool IsRunning();
private:
    void OnKey(int key, int scancode, int action, int mods);
    void OnResize(int width, int height);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
    wgpu::TextureView GetNextSurfaceTextureView();
private:
    GLFWwindow* m_window = nullptr;
    wgpu::Instance m_instance = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Queue m_queue = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::Adapter m_adapter = nullptr;
    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle;
private:
    int m_width = 0;
    int m_height = 0;
    std::unique_ptr<SparseDataVisualizer> m_visualizer;
};
