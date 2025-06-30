#pragma once
#include "ggl.h"

#include <webgpu/webgpu.hpp>
#include "transfer_function_widget.h"
#include "CameraController.h"
#include "test.h"
struct GLFWwindow;

class Application
{
public:
    bool OnInit(int width = 640, int height = 480, const char* title = "Learn WebGPU");
    void OnFrame();
    void OnFinish();
    void OnResize(int width, int height);
public:
    void MainLoop();
    bool IsRunning();
    void SetCameraController(std::unique_ptr<CameraController> controller) { m_cameraController = std::move(controller); }
private:
    bool InitWindowAndDevice(int width = 640, int height = 480, const char* title = "Learn WebGPU");
    void TerminateWindowAndDevice();
    bool InitSwapChain();
    void TerminateSwapChain();
    bool InitDepthBuffer();
    void TerminateDepthBuffer();
    bool InitGeometry();
    void TerminateGeometry();
    bool InitCameraAndControl();
    void TerminateCameraAndControl();
    bool InitGui();
    void TerminateGui();
    void UpdateGui(wgpu::RenderPassEncoder renderPass);
private:
    void OnKey(int key, int scancode, int action, int mods);
    void OnMouseMove(double xpos, double ypos);
    void OnMouseButton(int button, int action, int mods);
    void OnMouseScroll(double xoffset, double yoffset);
    
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
private:
    void OnTransferFunctionChanged();
    void UpdateRenderPipelineTransferFunction(wgpu::TextureView tfTextureView, wgpu::Sampler tfSampler);
    wgpu::TextureView GetNextSurfaceTextureView();
private:
    GLFWwindow* m_window = nullptr;
    wgpu::Instance m_instance = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Queue m_queue = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::Adapter m_adapter = nullptr;

    wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
    wgpu::Texture m_depthTexture = nullptr;
    wgpu::TextureView m_depthTextureView = nullptr;

private:
    int m_width = 0;
    int m_height = 0;
    std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;
    std::unique_ptr<TransferFunctionTest> m_tfTest;
    std::unique_ptr<CameraController> m_cameraController;
    std::unique_ptr<tfnw::WebGPUTransferFunctionWidget> m_transferFunctionWidget;
};