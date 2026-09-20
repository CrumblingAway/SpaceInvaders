#pragma once

#include <glm/glm.hpp>

class SDL_Window;

class Texture
{

};

class Shape
{

};

struct PlayerSpaceship
{
    /* Rendering. */
    Texture* sprite;

    /* Logic. */
    glm::vec3 position;
    Shape hitbox;
};

class SpaceInvadersGame
{
public:
    SpaceInvadersGame() = default;
    ~SpaceInvadersGame();

    /**
     * Init a game of space invaders. This method must be called before SpaceInfacersGame::Run.
     */
    bool Init();

    /**
     * Run a game of space invaders.
     */
    bool Run();

private:
    /**
     * Init SDL.
     * 
     * As this is the only windowing library this game accounts for, failure of this method results
     * in immediate termination of the application.
     */
    bool InitSDL();
    bool CleanupSDL();

    /**
     * Clean up the resources associated with this instance of the game.
     */
    bool Destroy();

    const char* m_pApplicationName = "Space Invaders";

    const size_t m_windowHeight = 720u;
    const size_t m_windowWidth = 1280u;
    SDL_Window* m_sdlWindow = nullptr;

    PlayerSpaceship m_playerSpaceship;
};
