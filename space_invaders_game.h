#pragma once

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
};
