cmake_minimum_required(VERSION 3.14)

add_library(kdtree SHARED
    src/kdtree.cpp       
)

target_include_directories(kdtree PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(kdtree PUBLIC cxx_std_17)  # 你需要的版本
