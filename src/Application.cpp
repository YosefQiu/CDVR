#include "Application.h"
#include "ComputeOptimizedVisualizer.h"
#include "GLFW/glfw3.h"
#include "webgpu-utils.h"

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <webgpu/webgpu.hpp>


bool Application::Initialize(int width, int height, const char* title)
{
// 初始化环境
// 1. 创建窗口（GLFW）
// 2. 创建 WebGPU 实例（Instance[类似于 OpenGL绑定 context]），并从窗口创建 Surface（渲染目标【类似于 OpenGL framebuffer】）
// 3. 请求适配器（Adapter）表示物理 GPU （选择 物理 GPU 作为渲染设备（Intel / AMD / NVIDIA））
// 4. 请求设备（Device） 表示逻辑 GPU +  获取队列（Queue）用于提交命令

	m_width = width;
	m_height = height;

    // Open window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);


    // 详细检查窗口创建结果
    int actualWidth, actualHeight;
    int framebufferWidth, framebufferHeight;
    float xscale, yscale;
    
    glfwGetWindowSize(m_window, &actualWidth, &actualHeight);
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    
    std::cout << "=== Window Creation Debug ===" << std::endl;
    std::cout << "Requested: " << width << "x" << height << std::endl;
    std::cout << "Actual window: " << actualWidth << "x" << actualHeight << std::endl;
    std::cout << "Framebuffer: " << framebufferWidth << "x" << framebufferHeight << std::endl;
    std::cout << "Content scale: " << xscale << "x" << yscale << std::endl;

    m_width = framebufferWidth;   // 这是关键！
    m_height = framebufferHeight;

	m_instance = wgpuCreateInstance(nullptr);

	std::cout << "Requesting adapter..." << std::endl;
	m_surface = glfwGetWGPUSurface(m_instance, m_window);
	wgpu::RequestAdapterOptions adapterOpts = {};
	adapterOpts.compatibleSurface = m_surface;
	wgpu::Adapter adapter = m_instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	m_instance.release();

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

    //  output
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
    std::cout << "Backend: " << backendStr << std::endl;

    std::cout << "Adapter name: " << properties.name << std::endl;
    std::cout << "Vendor ID: " << properties.vendorID << std::endl;
    std::cout << "Device ID: " << properties.deviceID << std::endl;
    std::cout << "Backend: " << backendStr << std::endl;

	uncapturedErrorCallbackHandle = m_device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	});

	m_queue = m_device.getQueue();

	// Configure the surface （默认的framebuffer）
	wgpu::SurfaceConfiguration config = {};
	// Configuration of the textures created for the underlying swap chain
	config.width = m_width;
	config.height = m_height;
	config.usage = wgpu::TextureUsage::RenderAttachment;
	wgpu::TextureFormat surfaceFormat = m_surface.getPreferredFormat(adapter);
	config.format = surfaceFormat;

	m_swapChainFormat = m_surface.getPreferredFormat(adapter);

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = m_device;
	config.presentMode = wgpu::PresentMode::Fifo;
	config.alphaMode = wgpu::CompositeAlphaMode::Auto;

	m_surface.configure(config);


	// 添加可视化器初始化1
	try {
        // 创建可视化器
        m_visualizer = std::make_unique<SparseDataVisualizer>(m_device, m_queue, m_cameraController->GetCamera());
        
		assert(m_visualizer);
        // 加载数据文件
        std::string dataPath = "pruned_simple_data.bin";  // 可以从命令行参数传入
        if (!m_visualizer->LoadFromBinary(dataPath)) {
            std::cerr << "Warning: Failed to load sparse data from " << dataPath << std::endl;
            m_visualizer.reset();  // 清空可视化器
        } 
        else 
        {
			// 设置摄像机
			if (m_cameraController)
				m_visualizer->SetCamera(m_cameraController->GetCamera());
            // 创建GPU资源
            m_visualizer->CreateBuffers(m_width, m_height);
            m_visualizer->CreatePipeline(surfaceFormat);
            
			// 初始化相机视图！
            float data_width = 150.0f;  // 从 visualizer 获取
            float data_height = 450.0f;
            Camera* camera = m_cameraController->GetCamera();
            camera->SetOrthoToFitContent(data_width, data_height, static_cast<float>(m_width) / static_cast<float>(m_height));
			camera->SetPosition(glm::vec3(0.0f, 0.0f, 1.0f));  
			camera->SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));
			camera->SetUp(glm::vec3(0.0f, 1.0f, 0.0f));
            
			m_visualizer->OnWindowResize(m_width, m_height);
			std::cout << "Sparse data visualizer initialized successfully" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing visualizer: " << e.what() << std::endl;
        m_visualizer.reset();
    }

    // 添加可视化器初始化2
	try {
        // 创建可视化器
        m_computeVisualizer = std::make_unique<ComputeOptimizedVisualizer>(m_device, m_queue, m_cameraController->GetCamera());

		assert(m_computeVisualizer);
        // 加载数据文件
        std::string dataPath = "pruned_simple_data.bin";  // 可以从命令行参数传入
        if (!m_computeVisualizer->LoadFromBinary(dataPath)) {
            std::cerr << "Warning: Failed to load sparse data from " << dataPath << std::endl;
            m_computeVisualizer.reset();  // 清空可视化器
        } 
        else 
        {
			// 设置摄像机
			if (m_cameraController)
				m_computeVisualizer->SetCamera(m_cameraController->GetCamera());
            // 创建GPU资源
            m_computeVisualizer->CreateBuffers(m_width, m_height);
            m_computeVisualizer->CreatePipeline(surfaceFormat);

			// 初始化相机视图！
            float data_width = 150.0f;  // 从 visualizer 获取
            float data_height = 450.0f;
            Camera* camera = m_cameraController->GetCamera();
            camera->SetOrthoToFitContent(data_width, data_height, static_cast<float>(m_width) / static_cast<float>(m_height));
			camera->SetPosition(glm::vec3(0.0f, 0.0f, 1.0f));  
			camera->SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));
			camera->SetUp(glm::vec3(0.0f, 1.0f, 0.0f));

			m_computeVisualizer->OnWindowResize(m_width, m_height);
			std::cout << "Compute optimized visualizer initialized successfully" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing visualizer: " << e.what() << std::endl;
        m_computeVisualizer.reset();
    }
	m_adapter = adapter;
	
	glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window, Application::KeyCallback);
	glfwSetCursorPosCallback(m_window, Application::CursorPosCallback);
	glfwSetMouseButtonCallback(m_window, Application::MouseButtonCallback);
	glfwSetScrollCallback(m_window, Application::ScrollCallback);
	glfwSetFramebufferSizeCallback(m_window, Application::FramebufferResizeCallback);
	
	if (!InitGui()) return false;

	return true;
}

void Application::Terminate()
{
	m_surface.unconfigure();
	m_queue.release();
	m_surface.release();
	m_device.release();

	TerminateGui();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void Application::MainLoop()
{

	// 主循环 （每一帧）
	// 1. 处理事件（GLFW）
	// 2. 获取当前帧的可绘制纹理视图（TextureView） 【相当于 OpenGL 中每帧隐式获得的「默认 framebuffer」】
	// 3. 创建命令编码器（CommandEncoder）
	// 4. 开启渲染通道（RenderPassEncoder）
	// 5. 清除背景颜色（通过 loadOp = Clear）
	// 6. 结束渲染通道，生成命令缓冲区（CommandBuffer）
	// 7. 提交命令缓冲区到 GPU 队列（Queue.submit）
	// 8. 显示结果（surface.present）—— 非浏览器环境下


	glfwPollEvents();
    if (m_cameraController) {
        m_cameraController->Update(1.0f / 60.0f);
    }
    if (m_visualizer && m_cameraController) {
        m_visualizer->SetCamera(m_cameraController->GetCamera());
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
    if (m_useCompute && m_computeVisualizer) {
        clearColor = wgpu::Color{ 0.0, 0.37, 0.54, 1.0 };  // 蓝绿色背景（Compute）
    } else if (m_visualizer) {
        clearColor = wgpu::Color{ 0.3, 0.3, 0.3, 1.0 };   // 深灰背景（Regular）
    } else {
        clearColor = wgpu::Color{ 0.1, 0.0, 0.0, 1.0 };   // 出错时背景（红色调）
    }

    renderPassColorAttachment.clearValue = clearColor;

   
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    
    // 重要：在主渲染通道开始时设置正确的viewport
    // std::cout << "Setting main viewport to: " << m_width << "x" << m_height << std::endl;
    renderPass.setViewport(0, 0, 
                          static_cast<float>(m_width),   // 使用framebuffer尺寸
                          static_cast<float>(m_height),  // 使用framebuffer尺寸
                          0.0f, 1.0f);

    // 渲染可视化内容
    if (m_useCompute && m_computeVisualizer) 
    {
        static bool printed = false;
        if (!printed) {
            std::cout << "Using compute optimized visualizer" << std::endl;
            printed = true;
        }

        m_computeVisualizer->UpdateUniforms(static_cast<float>(m_width) / static_cast<float>(m_height));
        m_computeVisualizer->Render(renderPass);
        
    } 
    else if (m_visualizer) 
    {
        static bool printed = false;
        if (!printed) {
            std::cout << "Using regular visualizer" << std::endl;
            printed = true;
        }

        // 使用常规可视化器
        m_visualizer->UpdateUniforms(static_cast<float>(m_width) / static_cast<float>(m_height));
        m_visualizer->Render(renderPass);
    } 
    else 
    {
        std::cerr << "No visualizer available to render!" << std::endl;
    }
    if (m_visualizer) 
    {
        float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
        m_visualizer->UpdateUniforms(aspectRatio);
        m_visualizer->Render(renderPass);
    } 

    renderPass.end();
    renderPass.release();

    // ===== 第二个渲染通道：ImGui =====
    wgpu::RenderPassColorAttachment imguiColorAttachment = {};
    imguiColorAttachment.view = targetView;
    imguiColorAttachment.resolveTarget = nullptr;
    imguiColorAttachment.loadOp = wgpu::LoadOp::Load;
    imguiColorAttachment.storeOp = wgpu::StoreOp::Store;
    imguiColorAttachment.clearValue = WGPUColor{ 0.0, 0.37, 0.54, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
    imguiColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    wgpu::RenderPassDescriptor imguiPassDesc = {};
    imguiPassDesc.colorAttachmentCount = 1;
    imguiPassDesc.colorAttachments = &imguiColorAttachment;
    imguiPassDesc.depthStencilAttachment = nullptr;
    imguiPassDesc.timestampWrites = nullptr;

    wgpu::RenderPassEncoder imguiPass = encoder.beginRenderPass(imguiPassDesc);
    
    // ImGui也使用framebuffer尺寸设置viewport
    // std::cout << "Setting ImGui viewport to: " << m_width << "x" << m_height << std::endl;
    imguiPass.setViewport(0, 0, 
                         static_cast<float>(m_width),   // 使用framebuffer尺寸
                         static_cast<float>(m_height),  // 使用framebuffer尺寸
                         0.0f, 1.0f);
    
    UpdateGui(imguiPass);
    imguiPass.end();
    imguiPass.release();

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
	} else if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
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
    std::cout << "OnResize called with framebuffer size: " << width << "x" << height << std::endl;
    
    // 这里的参数已经是framebuffer尺寸了（因为你用的是FramebufferResizeCallback）
    if (width <= 0 || height <= 0) {
        return;
    }
    
    // 检查是否真的需要重新配置
    if (m_width == width && m_height == height) {
        std::cout << "Size unchanged, skipping reconfigure" << std::endl;
        return;
    }
    
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
    
    std::cout << "Reconfiguring surface to: " << config.width << "x" << config.height << std::endl;
    m_surface.configure(config);

    // 通知可视化器窗口大小变化
    if (m_visualizer) {
        m_visualizer->OnWindowResize(width, height);  // 传递framebuffer尺寸
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
    
    std::cout << "ImGui DisplaySize: " << windowWidth << "x" << windowHeight << std::endl;
    std::cout << "ImGui FramebufferScale: " << scaleX << "x" << scaleY << std::endl;
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

    // Build our UI
    {
        // static bool m_useCompute = true;
        static bool show_demo_window = true;
        // static bool show_another_window = false;

        ImGui::Begin("Hello, world!");
        // ImGui::Text("This is some useful text.");
        ImGui::Checkbox("Demo Window", &show_demo_window);
        // ImGui::Checkbox("Another Window", &show_another_window);
        ImGui::Checkbox("Use Compute Shader", &m_useCompute);
        static int mode = 0;
        const char* items[] = { "Compute Shader", "Vertex+Fragment Shader" };
        ImGui::Combo("Render Mode", &mode, items, IM_ARRAYSIZE(items));
        m_useCompute = (mode == 0);

    




        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
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
                              static_cast<float>(1280), 
                              static_cast<float>(720), 
                              0.0f, 1.0f);
        
        // 渲染ImGui绘制数据
        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
    }
}
