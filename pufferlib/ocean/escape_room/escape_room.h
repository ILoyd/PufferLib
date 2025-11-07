#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include <math.h>

#define ROOM_SIZE 7
#define PIXEL_SIZE 64
#define TESTING 0

const unsigned char DOWN = 0;
const unsigned char UP = 1;
const unsigned char LEFT = 2;
const unsigned char RIGHT = 3;

const unsigned char EMPTY = 0;
const unsigned char WALL = 1;
const unsigned char AGENT = 2;
const unsigned char TARGET = 3;

// Required struct. Only use floats!
typedef struct {
    float perf; // Recommended 0-1 normalized single real number perf metric
    float score; // Recommended unnormalized single real number perf metric
    float episode_return; // Recommended metric: sum of agent rewards over episode
    float episode_length; // Recommended metric: number of steps of agent episode
    // Any extra fields you add here may be exported to Python in binding.c
    float n; // Required as the last field 
} Log;

typedef struct {
    Texture2D puffer;
    Texture2D wall;
} Client;

// Required that you have some struct for your env
// Recommended that you name it the same as the env file
typedef struct {
    Log log; // Required field. Env binding code uses this to aggregate logs
    unsigned char* observations; // Required. You can use any obs type, but make sure it matches in Python!
    int* actions; // Required. int* for discrete/multidiscrete, float* for box
    float* rewards; // Required
    unsigned char* terminals; // Required. We don't yet have truncations as standard yet
    int size;
    int tick;
    int x;
    int y;
    unsigned char* walls;
    unsigned char* tiles;
    int door_positions;
    int door;
    Client* client;
} EscapeRoom;

void add_log(EscapeRoom* env) {
    env->log.perf += (env->rewards[0] > 0) ? 1 : 0;
    env->log.score += env->rewards[0];
    env->log.episode_length += env->tick;
    env->log.episode_return += env->rewards[0];
    env->log.n++;
}

void init_cescape_room(EscapeRoom* env) {
    env->door_positions = env->size * 4 - 8;
    env->walls = (unsigned char*)calloc(env->door_positions, sizeof(unsigned char));
    env->tiles = (unsigned char*)calloc(env->size * env->size, sizeof(unsigned char));
}

void allocates_cescape_room(EscapeRoom* env) {
    env->observations = (unsigned char*)calloc(5, sizeof(unsigned char));
    env->actions = (int*)calloc(1, sizeof(int));
    env->rewards = (float*)calloc(1, sizeof(float));
    env->terminals = (unsigned char*)calloc(1, sizeof(unsigned char));
    init_cescape_room(env);
}

// Required function. Should clean up anything you allocated
// Do not free env->observations, actions, rewards, terminals
void c_close(EscapeRoom* env) {
    free(env->walls);
    free(env->tiles);
}

void free_cescape_room(EscapeRoom* env) {
    c_close(env);
    free(env->observations);
    free(env->actions);
    free(env->rewards);
}

Client* make_client(int cell_size, int width, int height) {
    InitWindow(width * cell_size, height * cell_size, "PufferLib Escape Room");
    SetTargetFPS(5);
    Client* client = (Client*)malloc(sizeof(Client));
    if (TESTING == 1) {
        client->wall = LoadTexture("../../resources/escape_room/wall.png");
        client->puffer = LoadTexture("../../resources/escape_room/puffers_128.png");
    }
    else {
        client->wall = LoadTexture("./resources/escape_room/wall.png");
        client->puffer = LoadTexture("./resources/escape_room/puffers_128.png");
    }
    return client;
}

void close_client(Client* client) {
    CloseWindow();
    UnloadTexture(client->puffer);
    UnloadTexture(client->wall);
    free(client);
}

void set_observations(EscapeRoom* env) {
    int doorX = env->door % env->size;
    int doorY = env->door / env->size;
    double distance = sqrt(pow(doorX - env->x, 2) + pow(doorY - env->y, 2));
    //printf("distance: %d", (int)distance);


    env->observations[0] = env->x;
    env->observations[1] = env->y;
    env->observations[2] = doorX;
    env->observations[3] = doorY;
    env->observations[4] = (int)distance;

    //printf("door.x: %d, door.y: %d\n", env->door % env->size, env->door / env->size);
}

// Required function
void c_reset(EscapeRoom* env) {
    int map_size = env->size * env->size;
    memset(env->tiles, 0, map_size * sizeof(unsigned char));
    memset(env->walls, 0, env->door_positions * sizeof(unsigned char));
    env->tiles[map_size / 2] = AGENT;
    env->x = env->size / 2;
    env->y = env->size / 2;
    env->tick = 0;
    int currentWallCount = 0;

    for (int i = 0; i < env->size; i++) {
        for (int j = 0; j < env->size; j++) {
            if (i % env->size == 0 || i % env->size == env->size - 1 || j % env->size == 0 || j % env->size == env->size - 1) {
                int index = i * env->size + j;
                env->tiles[index] = WALL;
                if (is_corner(index) != 1) {
                    env->walls[currentWallCount] = index;
                    currentWallCount++;
                }
            }
        }
    }

    int wall = rand() % env->door_positions;
    env->door = env->walls[wall];
    env->tiles[env->door] = TARGET;
    set_observations(env);
}

// Required function
void c_step(EscapeRoom* env) {
    env->tick += 1;

    int action = env->actions[0];
    env->terminals[0] = 0;
    env->rewards[0] = 0;

    env->tiles[env->y * env->size + env->x] = EMPTY;

    //printf("action: %d\n", action);

    if (action == DOWN) {
        env->y -= 1;
    }
    else if (action == RIGHT) {
        env->x += 1;
    }
    else if (action == UP) {
        env->y += 1;
    }
    else if (action == LEFT) {
        env->x -= 1;
    }

    int pos = env->y * env->size + env->x;
    if (env->tick > 3 * env->size) {
        env->terminals[0] = 1;
        env->rewards[0] = 0.0;
        add_log(env);
        c_reset(env);
        return;
    }
    else if (env->tiles[pos] == WALL) {
        env->terminals[0] = 1;
        env->rewards[0] = -1.0;
        add_log(env);
        c_reset(env);
        return;
    }
    else if (env->tiles[pos] == TARGET) {
        env->terminals[0] = 1;
        env->rewards[0] = 1.0;
        add_log(env);
        c_reset(env);
        return;
    }
    else {
        env->tiles[pos] = AGENT;
        set_observations(env);
    }
}

// Required function. Should handle creating the client on first call
void c_render(EscapeRoom* env) {

    // Standard across our envs so exiting is always the same
    if (IsKeyDown(KEY_ESCAPE)) {
        exit(0);
    }

    if (env->client == NULL) {
        env->client = make_client(PIXEL_SIZE, env->size, env->size);
    }

    Client* client = env->client;

    BeginDrawing();
    ClearBackground((Color) { 1, 123, 146, 255 });

    for (int i = 0; i < env->size; i++) {
        for (int j = 0; j < env->size; j++) {
            int tex = env->tiles[i * env->size + j];
            //const char* tileIndex = TextFormat("x: %d, y: %d", j, i);

            Color color = (Color){ 1, 123, 146, 255 };
            switch (tex) {
            case 1:
                DrawTexturePro(
                    client->wall,
                    (Rectangle) {
                    0, 0, 128, 128,
                },
                    (Rectangle) {
                    j* PIXEL_SIZE, i* PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE
                },
                    (Vector2) {
                    0, 0
                },
                    0,
                    WHITE
                );
                break;
            case 2:
                DrawTexturePro(
                    client->puffer,
                    (Rectangle) {
                    0, 0, 128, 128,
                },
                    (Rectangle) {
                    j* PIXEL_SIZE, i* PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE
                },
                    (Vector2) {
                    0, 0
                },
                    0,
                    WHITE
                );
                break;
            case 3:
                DrawRectangle(j * PIXEL_SIZE, i * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, color);
                break;
            default:
                DrawRectangleLines(j * PIXEL_SIZE, i * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, WHITE);
                break;
            }
            //DrawText(tileIndex, j* PIXEL_SIZE, i* PIXEL_SIZE, 5, WHITE);
        }
    }

    DrawRectangleLines((env->door % env->size) * PIXEL_SIZE, (env->door / env->size)* PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, RED);
    EndDrawing();
}

int is_corner(int tile) {
    if (tile == 0 || tile == ROOM_SIZE - 1 || tile == ROOM_SIZE * ROOM_SIZE - 1 || tile == ROOM_SIZE * ROOM_SIZE - ROOM_SIZE) {
        return 1;
    }
    return 0;
}