#pragma once

#include <cstddef>

// Forward declarations.
class SDL_Window;

/**
 * A Space Invaders clone. Only the `Run()` method is designed to be called from the outside. The
 * rest of the class is entirely internally managed.
 */
class SpaceInvadersGame
{
public:
    /**
     * Run the Space Invaders game. Calling this method hands off the application into the hands of
     * this class.
     */
    void Run();

private:

    /**
     * Init all dependencies.
     * 
     * As without any of the dependencies this application has no purpose, failure of this method
     * results in immediate termination of the application.
     */
    void Init();
    void Cleanup();

    /**
     * Init SDL.
     * 
     * As this is the only windowing library this game accounts for, failure of this method results
     * in immediate termination of the application.
     */
    void InitSDL();
    void CleanupSDL();

    /**
     * Init Vulkan.
     * 
     * As this is the only graphics API this game accounts for, failure  of this method results in
     * immediate termination of the application.
     */
    void InitVulkan();
    void CleanupVulkan();

    /**
     * This game's main loop. All rendering and game logic is handled in this method.
     */
    void MainLoop();

    const size_t m_windowHeight = 600;
    const size_t m_windowWidth = 800;
    SDL_Window* m_sdlWindow = nullptr;
};
