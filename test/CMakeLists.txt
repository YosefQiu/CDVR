cmake_minimum_required(VERSION 3.14)
project(app)

add_subdirectory(kdtree)         

add_executable(app main.cpp)
target_link_libraries(app PRIVATE kdtree)