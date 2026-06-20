cmake_minimum_required(VERSION 3.5)
project(Hello3D)

set(CMAKE_CXX_STANDARD 11)

add_executable(Hello3D src/main.cpp src/glad.c)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS} include src)

target_link_libraries(Hello3D ${OPENGL_LIBRARIES} glfw)

add_custom_command(TARGET Hello3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:Hello3D>/shaders)

add_custom_command(TARGET Hello3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Hello3D>/assets)
