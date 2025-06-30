cmake_minimum_required(VERSION 3.19)

PROJECT(app)

SET(CMAKE_CXX_STANDARD 17)

if (NOT EMSCRIPTEN)
	add_subdirectory(third/glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Enable compile_commands.json")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

add_subdirectory(third/webgpu)
add_subdirectory(third/glfw3webgpu)
add_subdirectory(third/imgui)
include_directories(third/glm)
include_directories(third/stb)


FILE(GLOB PROJECT_HEADERS include/*.h include/*.hpp)
FILE(GLOB PROJECT_SOURCES src/*.cpp)

file(COPY ${CMAKE_SOURCE_DIR}/pruned_simple_data.bin DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/shaders                 DESTINATION ${CMAKE_BINARY_DIR})

SOURCE_GROUP("Headers" FILES ${PROJECT_HEADERS})
SOURCE_GROUP("Sources" FILES ${PROJECT_SOURCES})



ADD_EXECUTABLE(
    ${PROJECT_NAME}
    ${PROJECT_HEADERS}
    ${PROJECT_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE glfw webgpu glfw3webgpu imgui)
target_copy_webgpu_binaries(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	COMPILE_WARNING_AS_ERROR OFF
)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -pedantic
    -Wunused-variable
    -Wno-error=unused-parameter
	-Wdelete-non-abstract-non-virtual-dtor
)
endif()

if (XCODE)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		XCODE_GENERATE_SCHEME ON
		XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
	)
endif()

if (EMSCRIPTEN)
	set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PRIVATE
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM"
        "--preload-file=pruned_simple_data.bin"
        "--preload-file=shaders"
    )
endif()
