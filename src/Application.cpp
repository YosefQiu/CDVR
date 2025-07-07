#include "Application.h"

#include "GLFW/glfw3.h"
#include "webgpu-utils.h"


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
        if (m_tfTest && m_visStyle == visStyle::k2D) 
        {
            m_tfTest->UpdateUniforms(vMat, pMat);
        }

        if (m_volumeRenderingTest && m_visStyle == visStyle::k3D) 
        {
            m_volumeRenderingTest->UpdateUniforms(vMat, pMat);
                
            // 添加旋转效果
            static float rotationAngle = 0.0f;
            rotationAngle += 0.01f;  // 每帧旋转0.01弧度
                
            glm::mat4 modelMatrix = glm::rotate(
                glm::mat4(1.0f), 
                rotationAngle, 
                glm::vec3(0.0f, 1.0f, 0.0f)
            );
            // m_volumeRenderingTest->SetModelMatrix(modelMatrix);
        }
    }
    
   if (m_transferFunctionWidget->changed()) 
   {
        wgpu::TextureView currentTFView = m_transferFunctionWidget->get_webgpu_texture_view();
        if (currentTFView) 
        {
            if (m_tfTest && m_visStyle == visStyle::k2D)
            {
                m_tfTest->UpdateSSBO(currentTFView);
            }
            if (m_volumeRenderingTest && m_visStyle == visStyle::k3D) 
            {
                m_volumeRenderingTest->UpdateSSBO(currentTFView);
            }
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

    if (m_tfTest && m_visStyle == visStyle::k2D) 
    {
        m_tfTest->Render(renderPass);
    }
    else if (m_volumeRenderingTest && m_visStyle == visStyle::k3D) 
    {
        m_volumeRenderingTest->Render(renderPass);
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
    else if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(m_window, true);
	} 
    else if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
        m_visStyle = visStyle::k2D;
        InitCameraAndControl();
    }
    else if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
        m_visStyle = visStyle::k3D;
        InitCameraAndControl();
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

    // m_cameraController->GetCamera()->SetViewportSize(width, height);
    if (m_visStyle == visStyle::k2D) 
    {
        // 2D模式：正交相机
        float data_width = 150.0f;
        float data_height = 450.0f;
        m_cameraController->GetCamera()->SetOrthoToFitContent(
            data_width, data_height, 
            static_cast<float>(width) / static_cast<float>(height)
        );
    } 
    else if (m_visStyle == visStyle::k3D) 
    {
        // 3D模式：透视相机
        m_cameraController->GetCamera()->SetPerspective(
            45.0f, 
            0.1f, 
            100.0f
        );
    }
    

    glm::mat4 viewMatrix = m_cameraController->GetCamera()->GetViewMatrix();
    glm::mat4 projMatrix = m_cameraController->GetCamera()->GetProjMatrix();
    if (m_tfTest && m_visStyle == visStyle::k2D)
        m_tfTest->OnWindowResize(viewMatrix, projMatrix);

    if (m_volumeRenderingTest && m_visStyle == visStyle::k3D) {
        // m_volumeRenderingTest->OnWindowResize(viewMatrix, projMatrix);
    }


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
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
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
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
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
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

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

wgpu::TextureView Application::GetNextSurfaceTextureView() 
{
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

        ImGui::Text("Data Type");
        int data_type = m_visStyle == visStyle::k2D ? 0 : 1; // 0 = 2D, 1 = 3D
        if (ImGui::RadioButton("2D", data_type == 0)) {
            data_type = 0;
            m_visStyle = visStyle::k2D;
            InitCameraAndControl();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("3D", data_type == 1)) {
            data_type = 1;
            m_visStyle = visStyle::k3D;
            InitCameraAndControl();
        }

        // 插值方法选择
        ImGui::Text("Interpolation Method");
        static int interpolation_method = 0; // 0 = KNN=1, 1 = KNN=3
        
        // 横向排列 radio buttons
        if (ImGui::RadioButton("KNN = 1", interpolation_method == 0)) {
            interpolation_method = 0;
            if (m_tfTest && m_visStyle == visStyle::k2D) m_tfTest->SetInterpolationMethod(interpolation_method);
        }
        ImGui::SameLine(); // 同一行显示下一个控件
        if (ImGui::RadioButton("KNN = 3", interpolation_method == 1)) {
            interpolation_method = 1;
            if (m_tfTest && m_visStyle == visStyle::k2D) m_tfTest->SetInterpolationMethod(interpolation_method);
        }
        ImGui::SameLine(); // 同一行显示下一个控件
        if (ImGui::RadioButton("KNN = 5", interpolation_method == 2)) {
            interpolation_method = 2;
            if (m_tfTest && m_visStyle == visStyle::k2D) m_tfTest->SetInterpolationMethod(interpolation_method);
        }


        ImGui::Text("Current K value: %d", interpolation_method == 0 ? 1 : interpolation_method == 1 ? 3 : 5);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Number of nearest neighbors used for interpolation");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // 搜索半径控制
        ImGui::Text("Search Radius");
        static float search_radius = 5.0f;  // 默认值，根据你的需求调整
        
        // 方式1: 滑动条 (推荐)
        auto max_range = 500.0f;
        if (ImGui::SliderFloat("##SearchRadius", &search_radius, 0.1f, max_range, "%.2f")) {
            // 当滑动条值改变时通知 m_tfTest
            if (m_tfTest && m_visStyle == visStyle::k2D) {
                m_tfTest->SetSearchRadius(search_radius);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Radius for searching nearby data points");
        }
        
        // 方式2: 可以同时提供输入框（可选）
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::InputFloat("##SearchRadiusInput", &search_radius, 0.1f, 1.0f, "%.2f")) {
            // 限制范围
            search_radius = std::max(0.1f, std::min(search_radius, 50.0f));
            if (m_tfTest && m_visStyle == visStyle::k2D) {
                m_tfTest->SetSearchRadius(search_radius);
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();

        // 可选：显示当前设置和帮助信息
        ImGui::Text("Current K value: %d", interpolation_method == 0 ? 1 : 3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Number of nearest neighbors used for interpolation");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // 直接在这个窗口中显示 Transfer Function
        if (show_transfer_function && m_transferFunctionWidget) {
            ImGui::Text("Transfer Function Controls");
            m_transferFunctionWidget->draw_ui();
            

            ImGui::Spacing();
            ImGui::Separator();
        }
        
        ImGui::End();
    }

    
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 window_pos = ImVec2(3.0f, io.DisplaySize.y - 3.0f);
		ImVec2 window_pos_pivot = ImVec2(0.0f, 1.0f);
		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
		if (ImGui::Begin("Performance Stats", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs))
		{
			ImGui::Text("%.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
			ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
			if (ImGui::IsMousePosValid())
			{
				ImGui::Text("Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
			}
			else {
				ImGui::Text("Mouse Position: <invalid>");
			}
		}
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

void Application::OnTransferFunctionChanged() 
{
    // 获取更新的颜色映射数据
    auto colormap = m_transferFunctionWidget->get_colormap();
    
    // 或者获取 WebGPU 纹理资源用于渲染
    wgpu::Texture tfTexture = m_transferFunctionWidget->get_webgpu_texture();
    wgpu::TextureView tfTextureView = m_transferFunctionWidget->get_webgpu_texture_view();
    wgpu::Sampler tfSampler = m_transferFunctionWidget->get_webgpu_sampler();

    if (m_tfTest && tfTextureView && m_visStyle == visStyle::k2D) 
    {
        m_tfTest->UpdateSSBO(tfTextureView);
    }

    if (m_volumeRenderingTest && tfTextureView) 
    {
        // m_volumeRenderingTest->UpdateSSBO(tfTextureView);
    }
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
        std::exit(EXIT_FAILURE);
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
    // std::cout << "Depth texture: " << m_depthTexture << std::endl;

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
    Camera* camera = nullptr;
    if (m_visStyle == visStyle::k2D) 
    {
        camera = new Camera(Camera::CameraMode::Ortho2D);
        float data_width = 150.0f;  // 从 visualizer 获取
        float data_height = 450.0f;
        camera->SetOrthoToFitContent(data_width, data_height, static_cast<float>(m_width) / static_cast<float>(m_height));
        camera->SetPosition(glm::vec3(0.0f, 0.0f, 1.0f));  
        camera->SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        camera->SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
    }
    else if (m_visStyle == visStyle::k3D)
    {
        camera = new Camera(Camera::CameraMode::Turntable3D);
        camera->SetViewportSize(m_width, m_height);
        camera->SetPerspective(45.0f, 0.1f, 100.0f);
        
        // 设置3D相机位置
        camera->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f));
        camera->SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        camera->SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
    }
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
    m_tfTest = std::make_unique<VIS2D>(m_device, m_queue, m_swapChainFormat);
    glm::mat4 viewMatrix = m_cameraController->GetCamera()->GetViewMatrix();
    glm::mat4 projMatrix = m_cameraController->GetCamera()->GetProjMatrix();
    m_tfTest->Initialize(viewMatrix, projMatrix);
    

    m_volumeRenderingTest = std::make_unique<VIS3D>(m_device, m_queue, m_swapChainFormat);
    m_volumeRenderingTest->Initialize(viewMatrix, projMatrix);
    
    if (!m_tfTest || !m_volumeRenderingTest) {
        std::cerr << "[ERROR]::InitGeometry() - Failed to create geometry tests!" << std::endl;
        return false;
    }

    return true;
}

void Application::TerminateGeometry()
{
    if (m_tfTest) {
        m_tfTest.reset();
    }

    if (m_volumeRenderingTest) {
        m_volumeRenderingTest.reset();
    }
}