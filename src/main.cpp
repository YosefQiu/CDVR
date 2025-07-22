#include "Application.h"
#include "Camera.hpp"
#include "CameraController.h"
#include <memory>


int main()
{
	
	// Initialize the application
	Application app;
	// Set the camera controller to the application
	

    try {
#ifdef __GNUC__
        if (!app.OnInit(1280 * 2, 760 * 2, "LearnWebGPU")) {
#elif defined(__APPLE__)
		if (!app.OnInit(1280, 760, "LearnWebGPU")) {
#endif
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
		pApp->OnFrame(); // 4. We can use the application object
	};
	emscripten_set_main_loop_arg(callback, &app, 0, true);
#else // __EMSCRIPTEN__
	while (app.IsRunning()) {
		app.OnFrame();
	}
#endif // __EMSCRIPTEN__

	app.OnFinish();

	return 0;
}
