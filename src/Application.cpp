#include "Application.h"

#include "GLFW/glfw3.h"
#include "webgpu-utils.h"


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
	config.width = width;
	config.height = height;
	config.usage = wgpu::TextureUsage::RenderAttachment;
	wgpu::TextureFormat surfaceFormat = m_surface.getPreferredFormat(adapter);
	config.format = surfaceFormat;

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = m_device;
	config.presentMode = wgpu::PresentMode::Fifo;
	config.alphaMode = wgpu::CompositeAlphaMode::Auto;

	m_surface.configure(config);


	// 添加可视化器初始化
	try {
        // 创建可视化器
        m_visualizer = std::make_unique<SparseDataVisualizer>(m_device, m_queue, m_cameraController->GetCamera());
        assert(m_visualizer);
        // 加载数据文件
        std::string dataPath = "pruned_simple_data.bin";  // 可以从命令行参数传入
        if (!m_visualizer->LoadFromBinary(dataPath)) {
            std::cerr << "Warning: Failed to load sparse data from " << dataPath << std::endl;
            m_visualizer.reset();  // 清空可视化器
        } else {
			// 设置摄像机
			if (m_cameraController)
				m_visualizer->SetCamera(m_cameraController->GetCamera());
            // 创建GPU资源
            m_visualizer->CreateBuffers(m_width, m_height);
            m_visualizer->CreatePipeline(surfaceFormat);
            
			// 重要：初始化相机视图！
            float data_width = 150.0f;  // 从 visualizer 获取
            float data_height = 450.0f;
            Camera* camera = m_cameraController->GetCamera();
            camera->SetOrtho(0.0f, data_width, 0.0f, data_height);
            camera->SetPosition(glm::vec3(data_width/2, data_height/2, 1.0f));
            camera->SetTarget(glm::vec3(data_width/2, data_height/2, 0.0f));
            
			m_visualizer->OnWindowResize(m_width, m_height);
			std::cout << "Sparse data visualizer initialized successfully" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing visualizer: " << e.what() << std::endl;
        m_visualizer.reset();
    }
	m_adapter = adapter;
	// Release the adapter only after it has been fully utilized
	// adapter.release();


	glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window, Application::KeyCallback);
	glfwSetCursorPosCallback(m_window, Application::CursorPosCallback);
	glfwSetMouseButtonCallback(m_window, Application::MouseButtonCallback);
	glfwSetScrollCallback(m_window, Application::ScrollCallback);
	glfwSetFramebufferSizeCallback(m_window, Application::FramebufferResizeCallback);
	
	std::cout << "Controller valid? " << (m_cameraController ? "YES" : "NO") << std::endl;
	return true;
}

void Application::Terminate()
{
	m_surface.unconfigure();
	m_queue.release();
	m_surface.release();
	m_device.release();
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
		m_cameraController->Update(1.0f / 60.0f);  // 或用 deltaTime
	}
	if (m_visualizer && m_cameraController) {
		m_visualizer->SetCamera(m_cameraController->GetCamera());
	}
	

	// Get the next target texture view
	wgpu::TextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	// Create a command encoder for the draw call
	wgpu::CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	wgpu::CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);

	// Create the render pass that clears the screen with our color
	wgpu::RenderPassDescriptor renderPassDesc = {};

	// The attachment part of the render pass descriptor describes the target texture of the pass
	wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
	renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
	renderPassColorAttachment.clearValue = WGPUColor{ 0.0, 0.37, 0.54, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	// Create the render pass and end it immediately (we only clear the screen but do not draw anything)
	wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
	
	// ===== 渲染可视化内容 =====
	if (m_visualizer) {
        // 更新uniforms（如果窗口大小改变了的话）
        float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
        m_visualizer->UpdateUniforms(aspectRatio);
        
        // 渲染
        m_visualizer->Render(renderPass);
    } 
	
	renderPass.end();
	renderPass.release();

	// Finally encode and submit the render pass
	wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	//std::cout << "Submitting command..." << std::endl;
	m_queue.submit(1, &command);
	command.release();
	//std::cout << "Command submitted." << std::endl;

	// At the enc of the frame
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
	m_width = width;
	m_height = height;

	// Update the surface configuration
	wgpu::SurfaceConfiguration config = {};
	config.width = width;
	config.height = height;
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
    // adapter.release();

	// 通知可视化器窗口大小变化
	if (m_visualizer) {
		m_visualizer->OnWindowResize(width, height);
	}
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
		if (app->m_cameraController) {
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
