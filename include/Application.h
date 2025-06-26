#pragma once
#include "ComputeOptimizedVisualizer.h"
#include "ggl.h"
#include "SparseDataVisualizer.h"
#include "CameraController.h"
#include <memory>
#include <webgpu/webgpu.hpp>
#include "transfer_function_widget.h"
struct GLFWwindow;

class Application
{
public:
    bool Initialize(int width = 640, int height = 480, const char* title = "Learn WebGPU");
    void Terminate();
    void MainLoop();
    bool IsRunning();
    void SetCameraController(std::unique_ptr<CameraController> controller) { m_cameraController = std::move(controller); }
private:
    bool InitGui();
    void TerminateGui();
    void UpdateGui(wgpu::RenderPassEncoder renderPass);
    void OnKey(int key, int scancode, int action, int mods);
    void OnMouseMove(double xpos, double ypos);
    void OnMouseButton(int button, int action, int mods);
    void OnMouseScroll(double xoffset, double yoffset);
    void OnResize(int width, int height);
    void OnTransferFunctionChanged();
    void UpdateRenderPipelineTransferFunction(wgpu::TextureView tfTextureView, wgpu::Sampler tfSampler);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
    wgpu::TextureView GetNextSurfaceTextureView();
private:
    GLFWwindow* m_window = nullptr;
    wgpu::Instance m_instance = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Queue m_queue = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::Adapter m_adapter = nullptr;
    wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Undefined;

    bool m_useCompute = true;
    std::unique_ptr<tfnw::WebGPUTransferFunctionWidget> m_transferFunctionWidget;
    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle;
private:
    int m_width = 0;
    int m_height = 0;
    std::unique_ptr<ComputeOptimizedVisualizer> m_computeVisualizer;
    std::unique_ptr<CameraController> m_cameraController;
};
