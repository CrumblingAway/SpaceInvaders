#include "space_invaders_game.h"

#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define PRINT_SDL_CRITICAL_ERROR(text)\
    std::cerr << "SDL: " << text << ". Error " << SDL_GetError() << ".\n";

SpaceInvadersGame::~SpaceInvadersGame()
{
    Destroy();
}

bool SpaceInvadersGame::Init()
{
    if (!InitSDL())
    {
        return false;
    }

    return true;
}

bool SpaceInvadersGame::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to initialize");
        return false;
    }
    if (SDL_Vulkan_LoadLibrary(NULL) < 0)
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to load Vulkan library");
        return false;
    }

    m_sdlWindow = SDL_CreateWindow(
        m_pApplicationName,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (m_sdlWindow == nullptr)
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to create window");
        return false;
    }

    return true;
}

bool SpaceInvadersGame::Run()
{
    bool quit{ false };
    while (!quit)
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            // Exit loop if the application is about to close
            if (event.type == SDL_EVENT_QUIT)
            {
                quit = true;
                break;
            }
        }
    }
    
    return true;
}

bool SpaceInvadersGame::Destroy()
{
    return true;
}

bool SpaceInvadersGame::CleanupSDL()
{
    SDL_DestroyWindow(m_sdlWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    return true;
}
