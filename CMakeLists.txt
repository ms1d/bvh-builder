cmake_minimum_required(VERSION 3.26)

project(bvh-builder)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB PATH_SRC src/*.cpp)
add_executable(bvh-builder ${PATH_SRC})

target_link_libraries(bvh-builder PRIVATE veclib)
target_include_directories(bvh-builder PRIVATE include)

add_subdirectory(lib)
