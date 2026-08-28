#include "space_invaders_game.h"

#include <iostream>

#include <SDL3/SDL.h>

void SpaceInvadersGame::Run()
{
    Init();
    MainLoop();
    Cleanup();
}

const char *SpaceInvadersGame::GetName() const
{
    return m_pApplicationName;
}

void SpaceInvadersGame::Init()
{
    InitSDL();
    InitVulkan();
}

void SpaceInvadersGame::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cerr << "Failed to initialize SDL. Error " << SDL_GetError() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    m_sdlWindow = SDL_CreateWindow(
        m_pApplicationName,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_VULKAN
    );
    if (m_sdlWindow == nullptr)
    {
        std::cerr << "Failed to create SDL window. Error " << SDL_GetError() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void SpaceInvadersGame::CleanupSDL()
{
    SDL_DestroyWindow(m_sdlWindow);
}

void SpaceInvadersGame::InitVulkan()
{

}

void SpaceInvadersGame::CleanupVulkan()
{

}

void SpaceInvadersGame::MainLoop()
{

}

void SpaceInvadersGame::Cleanup()
{
    CleanupVulkan();
    CleanupSDL();
}
