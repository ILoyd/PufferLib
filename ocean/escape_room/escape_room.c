#include "escape_room.h"

int main()
{
    EscapeRoom env = { .size = ROOM_SIZE };
    allocates_cescape_room(&env);

    c_reset(&env);
    c_render(&env);

    while (!WindowShouldClose()) {
        if (TESTING == 1) {
            if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) env.actions[0] = UP;
            if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) env.actions[0] = DOWN;
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) env.actions[0] = LEFT;
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) env.actions[0] = RIGHT;
        }
        else {
            env.actions[0] = rand() % 5;
        }

        c_step(&env);
        c_render(&env);
    }
    close_client(env.client);
    free_cescape_room(&env);
}