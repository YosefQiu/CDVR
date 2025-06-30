#include "Application.h"

#include "webgpu-utils.h"

#include "test.h"

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>



bool Application::OnInit(int width, int height, const char* title )
{
    if (!InitWindowAndDevice(width, height, title)) return false;
	if (!InitSwapChain()) return false;
	if (!InitDepthBuffer()) return false;
    if (!InitCameraAndControl()) return false;
	if (!InitGeometry()) return false;
	if (!InitGui()) return false;
	return true;
}

void Application::OnFrame()
{
    MainLoop();
}

void Application::OnFinish()
{
    TerminateGui();
    TerminateDepthBuffer();
    TerminateSwapChain();
    TerminateCameraAndControl();
    TerminateGeometry();
    TerminateWindowAndDevice();
}



void Application::MainLoop()
{

	glfwPollEvents();

    if (m_cameraController) 
    {
        m_cameraController->Update(1.0f / 60.0f);
        auto vMat = m_cameraController->GetCamera()->GetViewMatrix();
        auto pMat = m_cameraController->GetCamera()->GetProjMatrix();
        if (m_tfTest) 
        {
            m_tfTest->UpdateUniforms(vMat, pMat);
        }
    }
    
    if (m_tfTest && m_transferFunctionWidget->changed()) {
        wgpu::TextureView currentTFView = m_transferFunctionWidget->get_webgpu_texture_view();
        if (currentTFView) {
            m_tfTest->UpdateSSBO(currentTFView);
        }
    }


    wgpu::TextureView targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "My command encoder";
    wgpu::CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);

    // ===== 第一个渲染通道：主要内容 =====
    wgpu::RenderPassDescriptor renderPassDesc = {};
    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;

    wgpu::Color clearColor;
    clearColor = wgpu::Color{ 0.0, 0.0, 0.0, 1.0 };  // 蓝绿色背景（Compute）

    renderPassColorAttachment.clearValue = clearColor;

   
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = m_depthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false;
#ifdef WEBGPU_BACKEND_WGPU
    depthStencilAttachment.stencilClearValue = 0;
    depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Store;
    depthStencilAttachment.stencilReadOnly = true;
#else
    depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    depthStencilAttachment.stencilReadOnly = false;
#endif

    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
    renderPassDesc.timestampWrites = nullptr;

    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    
    // 重要：在主渲染通道开始时设置正确的viewport
    // std::cout << "Setting main viewport to: " << m_width << "x" << m_height << std::endl;
    renderPass.setViewport(0, 0, 
                          static_cast<float>(m_width),   // 使用framebuffer尺寸
                          static_cast<float>(m_height),  // 使用framebuffer尺寸
                          0.0f, 1.0f);

    if (m_tfTest) {
        m_tfTest->Render(renderPass);
    }
    
    
    UpdateGui(renderPass);

    

    renderPass.end();
    renderPass.release();

    wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();

    m_queue.submit(1, &command);
    command.release();

    targetView.release();
#ifndef __EMSCRIPTEN__
    m_surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    m_device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    m_device.poll(false);
#endif
}

bool Application::IsRunning() {
	return !glfwWindowShouldClose(m_window);
}

void Application::OnKey(int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(m_window, true);
	} 
    else if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
		// 切换全屏模式
		bool isFullscreen = glfwGetWindowMonitor(m_window) != nullptr;
		if (isFullscreen) {
			glfwSetWindowMonitor(m_window, nullptr, 0, 0, m_width, m_height, 0);
		} else {
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
	}
    
}

void Application::OnResize(int width, int height)
{
   
    if (width <= 0 || height <= 0) return;
    if (m_width == width && m_height == height) return;

    m_width = width;   // 这已经是framebuffer尺寸
    m_height = height;

    // Update the surface configuration
    wgpu::SurfaceConfiguration config = {};
    config.width = width;    // 使用framebuffer尺寸
    config.height = height;  // 使用framebuffer尺寸
    config.usage = wgpu::TextureUsage::RenderAttachment;
    
    wgpu::RequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = m_surface;
    wgpu::TextureFormat surfaceFormat = m_surface.getPreferredFormat(m_adapter);
    config.format = surfaceFormat;
    
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.device = m_device;
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;

    m_surface.configure(config);

    InitDepthBuffer();

    m_cameraController->GetCamera()->SetViewportSize(width, height);
    glm::mat4 viewMatrix = m_cameraController->GetCamera()->GetViewMatrix();
    glm::mat4 projMatrix = m_cameraController->GetCamera()->GetProjMatrix();
    m_tfTest->OnWindowResize(viewMatrix, projMatrix);


    // ImGui配置需要窗口逻辑尺寸，不是framebuffer尺寸
    int windowWidth, windowHeight;
    glfwGetWindowSize(m_window, &windowWidth, &windowHeight);
    
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight)); // 窗口逻辑尺寸
    
    // 设置正确的缩放比例
    float scaleX = static_cast<float>(width) / static_cast<float>(windowWidth);
    float scaleY = static_cast<float>(height) / static_cast<float>(windowHeight);
    io.DisplayFramebufferScale = ImVec2(scaleX, scaleY);
    
}

void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app) 
	{
		app->OnKey(key, scancode, action, mods);
		if (app->m_cameraController) 
		{
			app->m_cameraController->OnKeyPress(key, action);
		}
	}
}

void Application::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app) 
	{
		if (app->m_cameraController) 
		{
			app->m_cameraController->OnMouseMove(xpos, ypos);
		}
	}
}

void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app) {
		if (app->m_cameraController) 
		{
			app->m_cameraController->OnMouseButton(button, action, mods);
		}
	}
}

void Application::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app) {
		if (app->m_cameraController) {
			app->m_cameraController->OnMouseScroll(xoffset, yoffset);
		}
	}
}

void Application::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if (app) {
		app->OnResize(width, height);
	}
}

wgpu::TextureView Application::GetNextSurfaceTextureView() {
	// Get the surface texture
	wgpu::SurfaceTexture surfaceTexture;
	m_surface.getCurrentTexture(&surfaceTexture);
	if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
		return nullptr;
	}
	wgpu::Texture texture = surfaceTexture.texture;

	// Create a view for this surface texture
	wgpu::TextureViewDescriptor viewDescriptor;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = texture.getFormat();
	viewDescriptor.dimension = wgpu::TextureViewDimension::_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = wgpu::TextureAspect::All;
	wgpu::TextureView targetView = texture.createView(viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
	// We no longer need the texture, only its view
	// (NB: with wgpu-native, surface textures must not be manually released)
	wgpuTextureRelease(surfaceTexture.texture);
#endif // WEBGPU_BACKEND_WGPU

	return targetView;
}

bool Application::InitGui() 
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, m_depthTextureFormat);
	
    m_transferFunctionWidget = std::make_unique<tfnw::WebGPUTransferFunctionWidget>(
        m_device,  // 假设你的 WebGPU device 存储在 m_device 中
        m_queue
    );
    
    return true;
}

void Application::TerminateGui() 
{
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}


void Application::UpdateGui(wgpu::RenderPassEncoder renderPass) 
{
    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Build our UI - 只有一个主窗口
    {
        static bool show_demo_window = true;
        static bool show_transfer_function = true;

        
        ImGui::Begin("Application Control Panel");
        
        // 基本控制
        ImGui::Text("Render Settings");
        ImGui::Separator();
        
        ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::Checkbox("Show Transfer Function", &show_transfer_function);
        
        static int mode = 0;
        const char* items[] = { "Compute Shader", "Vertex+Fragment Shader" };
        ImGui::Combo("Render Mode", &mode, items, IM_ARRAYSIZE(items));
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // 直接在这个窗口中显示 Transfer Function
        if (show_transfer_function && m_transferFunctionWidget) {
            ImGui::Text("Transfer Function Controls");
            m_transferFunctionWidget->draw_ui();
            

            ImGui::Spacing();
            ImGui::Separator();
        }
        
        // 性能信息
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Performance");
        ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        
        ImGui::End();
    }

    // 渲染ImGui
    ImGui::EndFrame();
    ImGui::Render();

    // 获取实际的framebuffer尺寸（处理高DPI）
    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
        
    // 确保viewport尺寸正确且有效
    if (framebufferWidth > 0 && framebufferHeight > 0) {
        renderPass.setViewport(0, 0,
                               static_cast<float>(framebufferWidth),  // 修复：使用实际framebuffer尺寸
                               static_cast<float>(framebufferHeight), // 而不是硬编码的1280x720
                               0.0f, 1.0f);
                
        // 渲染ImGui绘制数据
        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
    }
}

void Application::OnTransferFunctionChanged() {
    // 获取更新的颜色映射数据
    auto colormap = m_transferFunctionWidget->get_colormap();
    
    // 或者获取 WebGPU 纹理资源用于渲染
    wgpu::Texture tfTexture = m_transferFunctionWidget->get_webgpu_texture();
    wgpu::TextureView tfTextureView = m_transferFunctionWidget->get_webgpu_texture_view();
    wgpu::Sampler tfSampler = m_transferFunctionWidget->get_webgpu_sampler();

    // 调试：检查 Transfer Function 数据
    std::cout << "=== Transfer Function Debug ===" << std::endl;
    std::cout << "Colormap size: " << colormap.size() << " bytes" << std::endl;
    std::cout << "Expected size for 256x1 RGBA: " << (256 * 4) << " bytes" << std::endl;
    
    // 检查前几个颜色值
    if (colormap.size() >= 12) {
        std::cout << "First 3 colors (RGBA):" << std::endl;
        for (int i = 0; i < 12; i += 4) {
            std::cout << "  Color " << (i/4) << ": (" 
                      << (int)colormap[i] << ", " 
                      << (int)colormap[i+1] << ", " 
                      << (int)colormap[i+2] << ", " 
                      << (int)colormap[i+3] << ")" << std::endl;
        }
    }
    
    std::cout << "Texture valid: " << (tfTexture ? "YES" : "NO") << std::endl;
    std::cout << "TextureView valid: " << (tfTextureView ? "YES" : "NO") << std::endl;
    std::cout << "===============================\n" << std::endl;
    
   

    
    UpdateRenderPipelineTransferFunction(tfTextureView, tfSampler);
    
    // std::cout << "Transfer function updated!" << std::endl;
}

void Application::UpdateRenderPipelineTransferFunction(wgpu::TextureView tfTextureView, wgpu::Sampler tfSampler)
{
    //用户调整TF → Widget更新纹理 → 绑定到Shader → GPU实时查找颜色
   if (m_tfTest && tfTextureView) {
        m_tfTest->UpdateSSBO(tfTextureView);
    }
    
    // std::cout << "Transfer function updated for compute visualization!" << std::endl;
}


bool Application::InitWindowAndDevice(int width, int height, const char* title)
{
	m_width = width;
	m_height = height;

	if (!glfwInit())
    {
        std::cerr << "[ERROR]::InitWindowAndDevice() failed to initialize GLFW!" << std::endl;
        return false;
    }
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window)
    {
        std::cerr << "[ERROR]::InitWindowAndDevice() failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    // 详细检查窗口创建结果
    int actualWidth, actualHeight;
    int framebufferWidth, framebufferHeight;
    float xscale, yscale;
    
    glfwGetWindowSize(m_window, &actualWidth, &actualHeight);
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    
    std::cout << "=== Window Creation Parameters ===" << std::endl;
    std::cout << "== Requested: " << width << "x" << height << std::endl;
    std::cout << "== Actual window: " << actualWidth << "x" << actualHeight << std::endl;
    std::cout << "== Framebuffer: " << framebufferWidth << "x" << framebufferHeight << std::endl;
    std::cout << "== Content scale: " << xscale << "x" << yscale << std::endl;

    m_width = framebufferWidth;   
    m_height = framebufferHeight;

	m_instance = wgpuCreateInstance(nullptr);
    if (!m_instance) {
        std::cerr << "[ERROR]::InitWindowAndDevice() failed to create WebGPU instance!" << std::endl;
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

	std::cout << "Requesting adapter..." << std::endl;
	m_surface = glfwGetWGPUSurface(m_instance, m_window);
	wgpu::RequestAdapterOptions adapterOpts = {};
	adapterOpts.compatibleSurface = m_surface;
	wgpu::Adapter adapter = m_instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

    wgpu::SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);

	std::cout << "Requesting device..." << std::endl;
	wgpu::DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
		std::cout << "Device lost: reason " << reason;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	m_device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << m_device << std::endl;

    wgpu::AdapterProperties properties;
    adapter.getProperties(&properties);
    std::string backendStr;
    switch (properties.backendType) {
        case WGPUBackendType_Metal: backendStr = "Metal"; break;
        case WGPUBackendType_Vulkan: backendStr = "Vulkan"; break;
        case WGPUBackendType_D3D12: backendStr = "Direct3D 12"; break;
        case WGPUBackendType_OpenGL: backendStr = "OpenGL"; break;
        default: backendStr = "Unknown"; break;
    }
    std::cout << "=== Adapter Properties ===" << std::endl;
    std::cout << "== Backend: " << backendStr << std::endl;
    std::cout << "== Adapter name: " << properties.name << std::endl;
    std::cout << "== Vendor ID: " << properties.vendorID << std::endl;
    std::cout << "== Device ID: " << properties.deviceID << std::endl;
    std::cout << "== Backend: " << backendStr << std::endl;

	m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	});

	m_queue = m_device.getQueue();
	m_adapter = adapter;
	
	glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window, Application::KeyCallback);
	glfwSetCursorPosCallback(m_window, Application::CursorPosCallback);
	glfwSetMouseButtonCallback(m_window, Application::MouseButtonCallback);
	glfwSetScrollCallback(m_window, Application::ScrollCallback);
	glfwSetFramebufferSizeCallback(m_window, Application::FramebufferResizeCallback);
	
	

	return m_device != nullptr;
}

void Application::TerminateWindowAndDevice()
{
    if (m_queue) {
        m_queue.release();
    }
    if (m_device) {
        m_device.release();
    }
    if (m_instance) {
        m_instance.release();
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

bool Application::InitSwapChain()
{
    if (!m_surface) {
        std::cerr << "[ERROR]::InitSwapChain() - Surface is not initialized!" << std::endl;
        return false;
    }

    wgpu::SurfaceConfiguration config = {};
	config.width = m_width;
	config.height = m_height;
	config.usage = wgpu::TextureUsage::RenderAttachment;
	wgpu::TextureFormat surfaceFormat = m_surface.getPreferredFormat(m_adapter);
	config.format = surfaceFormat;

	m_swapChainFormat = m_surface.getPreferredFormat(m_adapter);

	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = m_device;
	config.presentMode = wgpu::PresentMode::Fifo;
	config.alphaMode = wgpu::CompositeAlphaMode::Auto;

	m_surface.configure(config);

    
    return m_surface != nullptr;
}

void Application::TerminateSwapChain()
{
    if (m_surface) {
        m_surface.unconfigure();
        m_surface.release();
    }
    m_swapChainFormat = wgpu::TextureFormat::Undefined;
}

bool Application::InitDepthBuffer()
{
    if (!m_device) {
        std::cerr << "[ERROR]::InitDepthBuffer() - Device is not initialized!" << std::endl;
        return false;
    }

    wgpu::TextureDescriptor depthTextureDesc = {};
    depthTextureDesc.label = "Depth Texture";
    depthTextureDesc.size.width = m_width;
    depthTextureDesc.size.height = m_height;
    depthTextureDesc.size.depthOrArrayLayers = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.dimension = wgpu::TextureDimension::_2D;
    depthTextureDesc.format = m_depthTextureFormat;
    depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
    m_depthTexture = m_device.createTexture(depthTextureDesc);
    std::cout << "Depth texture: " << m_depthTexture << std::endl;

    wgpu::TextureViewDescriptor depthViewDesc = {};
    depthViewDesc.label = "Depth Texture View";
    depthViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
	depthViewDesc.baseArrayLayer = 0;
	depthViewDesc.arrayLayerCount = 1;
	depthViewDesc.baseMipLevel = 0;
	depthViewDesc.mipLevelCount = 1;
	depthViewDesc.dimension = wgpu::TextureViewDimension::_2D;
	depthViewDesc.format = m_depthTextureFormat;
    m_depthTextureView = m_depthTexture.createView();
    // std::cout << "Depth texture view: " << m_depthTextureView << std::endl;
    
    return m_depthTextureView != nullptr;
}

void Application::TerminateDepthBuffer()
{
    if (m_depthTextureView) {
        m_depthTextureView.release();
        m_depthTextureView = nullptr;
    }
    if (m_depthTexture) {
        m_depthTexture.release();
        m_depthTexture = nullptr;
    }
    m_depthTextureFormat = wgpu::TextureFormat::Undefined;
}


bool Application::InitCameraAndControl()
{
    Camera* camera = new Camera(Camera::CameraMode::Ortho2D);
    float data_width = 150.0f;  // 从 visualizer 获取
    float data_height = 450.0f;
    camera->SetOrthoToFitContent(data_width, data_height, static_cast<float>(m_width) / static_cast<float>(m_height));
	camera->SetPosition(glm::vec3(0.0f, 0.0f, 1.0f));  
	camera->SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));
	camera->SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
    m_cameraController = std::make_unique<CameraController>(camera);
    if (!m_cameraController) {
        std::cerr << "[ERROR]::InitCameraAndControl() - Failed to create CameraController!" << std::endl;
        return false;
    }
    SetCameraController(std::move(m_cameraController));

    


    return m_cameraController != nullptr;
}

void Application::TerminateCameraAndControl()
{
    if (m_cameraController) {
        m_cameraController.reset();
    }
}

bool Application::InitGeometry()
{
    m_tfTest = std::make_unique<TransferFunctionTest>(m_device, m_queue, m_swapChainFormat);
    glm::mat4 viewMatrix = m_cameraController->GetCamera()->GetViewMatrix();
    glm::mat4 projMatrix = m_cameraController->GetCamera()->GetProjMatrix();
    m_tfTest->Initialize(viewMatrix, projMatrix);
    return m_tfTest != nullptr;
}

void Application::TerminateGeometry()
{
    if (m_tfTest) {
        m_tfTest.reset();
    }
}