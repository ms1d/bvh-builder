cmake_minimum_required(VERSION 3.26)

project(bvh-builder)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB CLIENT_SRC src/client/*.cpp)
add_executable(bvh-builder ${CLIENT_SRC})

file(GLOB DAEMON_SRC src/daemon/*.cpp)
add_executable(bvh-builderd ${DAEMON_SRC})

include(FetchContent)

FetchContent_Declare(
    veclib
    GIT_REPOSITORY https://github.com/ms1d/veclib
	GIT_TAG main
)

FetchContent_Declare(
    commonlib
    GIT_REPOSITORY https://github.com/ms1d/commonlib
	GIT_TAG main
)

FetchContent_Declare(
    thread-poollib
    GIT_REPOSITORY https://github.com/ms1d/thread-poollib
	GIT_TAG main
)

FetchContent_Declare(
	memory-poollib
	GIT_REPOSITORY https://github.com/ms1d/memory-poollib
	GIT_TAG main
)

FetchContent_MakeAvailable(veclib commonlib thread-poollib memory-poollib)

target_link_libraries(bvh-builderd PRIVATE veclib thread-poollib memory-poollib)
target_include_directories(bvh-builderd PRIVATE include)
target_include_directories(bvh-builder PRIVATE include)
