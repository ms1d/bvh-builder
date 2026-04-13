cmake_minimum_required(VERSION 3.26)

file(GLOB PATH_SRC src/*.cpp)
add_executable(bvh-builder ${PATH_SRC})

target_link_libraries(bvh-builder PRIVATE vec)
