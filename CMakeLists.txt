cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 23)

project(space_invaders)

add_executable(${PROJECT_NAME} space_invaders_game.cpp main.cpp vma_usage.cpp tiny_obj_usage.cpp volk_usage.cpp)

target_link_libraries(${PROJECT_NAME}
	/usr/local/lib/libSDL3.so
	/usr/local/lib/libvulkan.so
	/usr/local/lib/libktx.so	
	/usr/local/lib/libslang.so
)
