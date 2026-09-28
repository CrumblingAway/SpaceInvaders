#include "space_invaders_game.h"

int main()
{
    SpaceInvadersGame game;
    if (!game.Init())
    {
        return 1;
    }
    game.Run();

    return 0;
}
