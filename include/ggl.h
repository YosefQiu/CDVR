#pragma once

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <fstream>  
#include <sstream>
#include <limits>
#include <cstdint>
#include <cstring>  
#include <unordered_map>
#include <optional>
#include <iomanip>
#include <list>
#include <numeric>
#include <random>
#include <chrono>
#include <variant>
#include <functional>
#include <memory>



#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>


#include <webgpu/webgpu.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__
