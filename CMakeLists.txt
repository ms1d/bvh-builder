cmake_minimum_required(VERSION 3.26)

project(bvh-builder)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB CLIENT_SRC src/client/*.cpp)
add_executable(bvh-builder ${CLIENT_SRC})

file(GLOB DAEMON_SRC src/daemon/*.cpp)
add_executable(bvh-builderd ${DAEMON_SRC})

target_link_libraries(bvh-builderd PRIVATE veclib)
target_include_directories(bvh-builderd PRIVATE include)

add_subdirectory(lib)
