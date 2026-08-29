cmake_minimum_required(VERSION 4.3)

project(space_invaders)

add_executable(${PROJECT_NAME} space_invaders_game.cpp main.cpp vk_mem_alloc.h)

target_link_libraries(${PROJECT_NAME} /usr/local/lib/libSDL3.so /usr/local/lib/libvulkan.so)

