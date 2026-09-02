cmake_minimum_required(VERSION 3.26)

project(bvh-builder)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(MY_DEPS
    veclib
    commonlib
    thread-poollib
    memory-poollib
)

foreach(dep IN LISTS MY_DEPS)
    FetchContent_Declare(
        ${dep}
        GIT_REPOSITORY https://github.com/ms1d/${dep}
        GIT_TAG main
    )
endforeach()

FetchContent_MakeAvailable(${MY_DEPS})

if(USE_ASAN)
	add_compile_options(-fsanitize=address,undefined)
	add_link_options(-fsanitize=address,undefined)
elseif(USE_TSAN)
	add_compile_options(-fsanitize=thread)
	add_link_options(-fsanitize=thread)
endif()

if(ADD_SYMBOLS)
	add_compile_options(-g)
	add_link_options(-g)
endif()

file(GLOB CLIENT_SRC src/client/*.cpp)
add_executable(bvh-builder ${CLIENT_SRC})

file(GLOB DAEMON_SRC src/daemon/*.cpp)
add_executable(bvh-builderd ${DAEMON_SRC})

target_link_libraries(bvh-builderd PRIVATE veclib thread-poollib memory-poollib)
target_include_directories(bvh-builderd PRIVATE include)
target_include_directories(bvh-builder PRIVATE include)
