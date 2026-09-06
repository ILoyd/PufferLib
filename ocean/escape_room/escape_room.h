#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"

#define TESTING 0
#define ROOM_SIZE 13
#define PIXEL_SIZE 64
#define MAX_STEP 100

const unsigned char NONE = 0;
const unsigned char UP = 1;
const unsigned char DOWN = 2;
const unsigned char LEFT = 3;
const unsigned char RIGHT = 4;

const unsigned char EMPTY = 0;
const unsigned char WALL = 1;
const unsigned char AGENT = 2;
const unsigned char TARGET = 3;
const unsigned char KEY = 4;

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

typedef struct{
    int x;
    int y;
} Goal;

// Required that you have some struct for your env
// Recommended that you name it the same as the env file
typedef struct {
    Log log; // Required field. Env binding code uses this to aggregate logs
    Client* client;
    float* observations; // Required. You can use any obs type, but make sure it matches in Python!
    float* actions; // Required
    float* rewards; // Required
    float* terminals; // Required
    unsigned char* tiles;
    Goal* goals;
    bool is_picked_up;
    int num_agents;
    int size;
    int step_count;
    int x;
    int y;
    unsigned int rng;
} EscapeRoom;

void add_log(EscapeRoom* env) {
    env->log.perf += (env->rewards[0] > 0) ? 1 : 0;
    env->log.score += env->rewards[0];
    env->log.episode_length += env->step_count;
    env->log.episode_return += env->rewards[0];
    env->log.n++;
}

Client* make_client() {
    Client* client = (Client*)malloc(sizeof(Client));
    if (TESTING == 1) {
        client->wall = LoadTexture("./resources/escape_room/wall.png");
        client->puffer = LoadTexture("./resources/escape_room/puffers_128.png");
    }
    else {
        client->wall = LoadTexture("./resources/escape_room/wall.png");
        client->puffer = LoadTexture("./resources/escape_room/puffers_128.png");
    }
    return client;
}

void close_client(Client* client) {
    UnloadTexture(client->puffer);
    UnloadTexture(client->wall);
    CloseWindow();
    free(client);
}

void init_cescape_room(EscapeRoom* env) {
    env->tiles = (unsigned char*)calloc(env->size * env->size, sizeof(unsigned char));
    env->goals = calloc(2, sizeof(Goal));
    env->is_picked_up = true;
}

int calc_distance(EscapeRoom* env, Goal* goal) {
    return abs(env->x - goal->x) + abs(env->y - goal->y);
}

void set_observations(EscapeRoom* env) {
    Goal* door = &env->goals[0];

    env->observations[0] = env->y * env->size + env->x;

    env->observations[1] = door->y * env->size + door->x; 
    Goal* key = &env->goals[1];
    env->observations[2] = key->y * env->size + key->x;
    env->observations[3] = calc_distance(env, door);  
    env->observations[4] = (float)env->is_picked_up;
}

void set_door_pos(EscapeRoom* env, bool randomSide, bool randomPos) {
    int side = -1;
    int pos = 0;
    int door_pos = 0;

    if (randomSide) {
        side = rand_r(&env->rng) % 4;
    }

    if (randomPos) {
        pos = 1 + rand_r(&env->rng) % (env->size - 2);
    }

    Goal* door = &env->goals[0];

    switch (side) {
    case 0:
        door->y = 0;
        door->x = pos;
        break;
    case 1:
        door->y = env->size - 1;
        door->x = pos;
        break;
    case 2:
        door->y = pos;
        door->x = 0;
        break;
    case 3:
        door->y = pos;
        door->x = env->size - 1;
        break;
    default:
        door->y = pos;
        door->x = env->size - 1;
        break;
    }

    door_pos = door->y * env->size + door->x;
    env->tiles[door_pos] = TARGET;
}

// Required function
void c_reset(EscapeRoom* env) {
    int map_size = env->size * env->size;
    memset(env->tiles, 0, map_size*sizeof(unsigned char));
    env->tiles[map_size / 2] = AGENT;
    env->x = env->size / 2;
    env->y = env->size / 2;
    env->step_count = 0;
    env->is_picked_up = false;
    Goal* key = &env->goals[1];
    key->x = env->size / 4;
    key->y = env->size / 4; 
    int key_index = key->y * env->size + key->x;
    env->tiles[key_index] = KEY;

    int index = 0;
    for (int i = 0; i < env->size; i++) {
        for (int j = 0; j < env->size; j++) {
            index = i * env->size + j;
            if (i % env->size == 0 || i % env->size == env->size - 1 || j % env->size == 0 || j % env->size == env->size - 1) {
                env->tiles[index] = WALL;
            }
        }
    }

    set_door_pos(env, true, true);
    set_observations(env);
}

// Required function
void c_step(EscapeRoom* env) {
    env->step_count += 1;
    int action = (int)env->actions[0];
    env->terminals[0] = 0;
    env->rewards[0] = 0;

    env->tiles[env->y * env->size + env->x] = EMPTY;

    if (action == DOWN) {
        env->y += 1;
    }
    else if (action == RIGHT) {
        env->x += 1;
    }
    else if (action == UP) {
        env->y -= 1;
    }
    else if (action == LEFT) {
        env->x -= 1;
    }

    int pos = env->y * env->size + env->x;

    if(env->tiles[pos] == KEY){
        env->is_picked_up = true;
        env->tiles[pos] = EMPTY;
        env->rewards[0] = 0.5f;
        Goal* door = &env->goals[0];
        env->tiles[door->y * env->size + door->x] = TARGET;
    }
    else if (env->tiles[pos] == TARGET) {
        env->terminals[0] = 1;
        if(env->is_picked_up == true){
            env->rewards[0] = 1.0f;
        }
        else{
            env->rewards[0] = -1.0f;
        }

        add_log(env);
        c_reset(env);
        return;
    }
    else if (env->tiles[pos] == WALL || env->step_count >= MAX_STEP) {
        env->terminals[0] = 1;
        env->rewards[0] = -1.0f;
        add_log(env);
        c_reset(env);
        return;
    }

    env->tiles[pos] = AGENT;
    env->rewards[0] = 0.0f;
    set_observations(env);
}

// Required function. Should handle creating the client on first call
void c_render(EscapeRoom* env) {
    if (env->client == NULL) {
        InitWindow(env->size * PIXEL_SIZE, env->size * PIXEL_SIZE, "PufferLib Escape Room");
        SetTargetFPS(5);
        env->client = make_client();
    }

    // Standard across our envs so exiting is always the same
    if (IsKeyDown(KEY_ESCAPE)) {
        exit(0);
    }

    Client* client = env->client;

    BeginDrawing();
    ClearBackground((Color) { 1, 123, 146, 255 });

    for (int i = 0; i < env->size; i++) {
        for (int j = 0; j < env->size; j++) {
            int tex = env->tiles[i * env->size + j];

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
            case 4:
                DrawCircle(j * PIXEL_SIZE + (PIXEL_SIZE / 2), i * PIXEL_SIZE + (PIXEL_SIZE / 2), 20, (Color){ 144, 238, 144, 255 });
                break;
            default:
                DrawRectangleLines(j * PIXEL_SIZE, i * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, WHITE);
                break;
            }
        }
    }

    
    if(env->is_picked_up){
        Goal* door = &env->goals[0];
        DrawRectangleLines((door->x) * PIXEL_SIZE, (door->y) * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, RED);
    }
    EndDrawing();
}

// Required function. Should clean up anything you allocated
// Do not free env->observations, actions, rewards, terminals
void c_close(EscapeRoom* env) {
    free(env->tiles);
    free(env->goals);
    if (env->client != NULL) {
        close_client(env->client);
    }
}

void allocates_cescape_room(EscapeRoom* env) {
    env->observations = (float*)calloc(5, sizeof(float));
    env->actions = (float*)calloc(1, sizeof(float));
    env->rewards = (float*)calloc(1, sizeof(float));
    env->terminals = (float*)calloc(1, sizeof(float));
    init_cescape_room(env);
}

void free_cescape_room(EscapeRoom* env) {
    free(env->observations);
    free(env->actions);
    free(env->rewards);
    free(env->terminals);
    c_close(env);
}