cmake_minimum_required(VERSION 4.3)

project(space_invaders)

add_executable(${PROJECT_NAME} space_invaders_game.cpp main.cpp vma_usage.cpp tiny_obj_usage.cpp)

target_link_libraries(${PROJECT_NAME}
	/usr/local/lib/libSDL3.so
	/usr/local/lib/libvulkan.so
)
