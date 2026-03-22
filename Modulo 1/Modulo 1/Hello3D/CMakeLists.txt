cmake_minimum_required(VERSION 3.5)
project(Hello3D)

set(CMAKE_CXX_STANDARD 11)

add_executable(Hello3D src/main.cpp src/glad.c)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS} include)

target_link_libraries(Hello3D ${OPENGL_LIBRARIES} glfw)
