#pragma once

#include <glm/glm.hpp>

class Texture;

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
     * Clean up the resources associated with this instance of the game.
     */
    bool Destroy();

    PlayerSpaceship m_playerSpaceship;
};
