#pragma once
#include "ggl.h"

#include "TFWidget.h"
#include "CameraController.h"
#include "VIS2D.h"
#include "VIS3D.h"
struct GLFWwindow;

class Application
{
    enum class visStyle {k2D, k3D};
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
    void OnTransferFunctionChanged();
    
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
private:
    
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
    visStyle m_visStyle = visStyle::k3D;
    std::unique_ptr<CameraController> m_cameraController;
    std::unique_ptr<tfnw::WebGPUTransferFunctionWidget> m_transferFunctionWidget;
    std::unique_ptr<VIS3D> m_volumeRenderingTest;
    std::unique_ptr<VIS2D> m_tfTest;
};