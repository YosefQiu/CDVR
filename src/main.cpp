#include "Application.h"
#include "Camera.hpp"
#include "CameraController.h"
#include <memory>


int main()
{
	Camera* camera = new Camera(Camera::CameraMode::Ortho2D);
	std::unique_ptr<CameraController> cameraController = std::make_unique<CameraController>(camera);

	

	// Initialize the application
	Application app;
	// Set the camera controller to the application
	app.SetCameraController(std::move(cameraController));


    try {
        if (!app.Initialize(1280, 760, "LearnWebGPU")) {
            throw std::runtime_error("Failed to initialize application.");
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return 1;
    }


#ifdef __EMSCRIPTEN__
	// Equivalent of the main loop when using Emscripten:
	auto callback = [](void *arg) {
		Application* pApp = reinterpret_cast<Application*>(arg);
		pApp->MainLoop(); // 4. We can use the application object
	};
	emscripten_set_main_loop_arg(callback, &app, 0, true);
#else // __EMSCRIPTEN__
	while (app.IsRunning()) {
		app.MainLoop();
	}
#endif // __EMSCRIPTEN__

	app.Terminate();

	return 0;
}
