/*******************************************************************************************
*
*   Montana Tech Miner - SE322 Prototype
*   Based on RayLib 2D Challenge by Hans de Ruiter
*
********************************************************************************************/

#include "raylib.h"
#include <stdbool.h> 
#include <stdio.h>

/* ── #define only where required by the compiler ────────────────────────────
   These values are used to declare array sizes inside a struct in Assignment 2.
   C requires array dimensions to be compile-time constants.
   static const int does NOT qualify as a compile-time constant in C.        */

#define TILE_SIZE    10
#define SCREEN_W    480
#define SCREEN_H    270
#define MAP_COLS    (SCREEN_W / TILE_SIZE)   /* 48 */
#define MAP_ROWS    (SCREEN_H / TILE_SIZE)   /* 27 */

// adding define for stamina
#define PLAYER_MAX_STAMINA 100.0f
#define JUMP_STAMINA_COST  15.0f
#define STAMINA_FROM_FOOD  40.0f

// stamina bar 
#define STAMINA_BAR_X      15
#define STAMINA_BAR_Y      15
#define STAMINA_BAR_WIDTH  100
#define STAMINA_BAR_HEIGHT 15

/* ── static const for everything else ───────────────────────────────────────
   These values are only used in runtime calculations.
   static const gives them a type, a name visible in the debugger,
   and limits their scope to this file.                                       */

static const int   PLAYER_W   = TILE_SIZE;        /* 10 px – 1 tile wide   */
static const int   PLAYER_H   = TILE_SIZE * 3;    /* 30 px – 3 tiles tall  */
// static const int   PLAYER_SPD = 2;                /* pixels per frame      */
static const float GRAVITY    = 0.4f;             /* px / frame²           */
// static const float JUMP_FORCE = -7.5f;            /* px / frame, upward    */
static const float MAX_FALL   = 9.0f;             /* must stay < TILE_SIZE */

// adding const for stamina
static const float BASE_SPD = 2.0f; /* base pixels for per frame */
static const float BASE_JUMP = -7.5; /* base jump velocity*/




typedef enum {
    TILE_EMPTY = 0,   /* air  – nothing drawn, player falls through */
    TILE_EARTH,        /* dirt – drawn as rectangle, solid ground     */
    TILE_FOOD       // stamina food added
} TileType;

typedef struct {
    TileType tiles[MAP_ROWS][MAP_COLS];
} GameMap;

static bool TileSolid(const GameMap *m, int col, int row)
{
    if (col < 0 || col >= MAP_COLS) return true;
    if (row < 0 || row >= MAP_ROWS) return true;
    return m->tiles[row][col] != TILE_EMPTY;
}

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float stamina;
    bool onGround;
    bool facingRight;
} Player;

static void MapInit(GameMap *m)
{
/* step 1: fill everything with air */
    for (int r = 0; r < MAP_ROWS; r++){
        for (int c = 0; c < MAP_COLS; c++){
            m->tiles[r][c] = TILE_EMPTY;
        }
    }

    /* step 2: solid floor – rows 21 to 26 */
    int floorRow = (MAP_ROWS * 4) / 5;   /* = 21 */
    
    for (int r = floorRow; r < MAP_ROWS; r++){
        for (int c = 0; c < MAP_COLS; c++){
            m->tiles[r][c] = TILE_EARTH;
        }
    }

    // step 3: food square placed above the floor. if it's on the floor the player doesn't eat it
    m->tiles[floorRow - 2][10] = TILE_FOOD;
    m->tiles[floorRow - 2][20] = TILE_FOOD;
    m->tiles[floorRow - 2][30] = TILE_FOOD;
}

static void MapDraw(const GameMap *m)
{
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++) {
            if (m->tiles[r][c] == TILE_EMPTY) continue;

            Color tileColor = (m->tiles[r][c] == TILE_EARTH) ? DARKBROWN : RED; // added for the food

            DrawRectangle(
                c * TILE_SIZE,
                r * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                tileColor
            );
        }
}
//

// helper funtions for stamina and food
static void RegenerateStaminaFromFood(Player *player, GameMap *map)
{
    int col = (int)((player->position.x + (PLAYER_W / 2.0f)) / TILE_SIZE);
    int row = (int)((player->position.y + (PLAYER_H / 2.0f)) / TILE_SIZE);

    if (col >= 0 && col < MAP_COLS && row >= 0 && row < MAP_ROWS) {
        if (map->tiles[row][col] == TILE_FOOD) {
            player->stamina += STAMINA_FROM_FOOD;
            if (player->stamina > PLAYER_MAX_STAMINA) {
                player->stamina = PLAYER_MAX_STAMINA;
            }
            map->tiles[row][col] = TILE_EMPTY; /* consume food */
        }
    }
}

static void UpdatePlayerStamina(Player *player, bool attemptedJump)
{
    if (attemptedJump) {
        if (player->stamina >= JUMP_STAMINA_COST) {
            player->stamina -= JUMP_STAMINA_COST;
            if (player->stamina < 0.0f) player->stamina = 0.0f;
        }
    }
}

static void DrawDebugStamina(const Player *player)
{
    char debugBuf[64];
    snprintf(debugBuf, sizeof(debugBuf), "Stamina: %.1f / %.1f", player->stamina, PLAYER_MAX_STAMINA);
    DrawText(debugBuf, 15, 15, 18, WHITE);
}
// end helper funtions for stamina and food

// stamina bar logic
static void DrawStaminaBar(const Player *player)
{
    // 1. Dynamic fill width calculation based on current stamina percentage 
    float staminaPct = player->stamina / PLAYER_MAX_STAMINA;
    int fillWidth = (int)(staminaPct * STAMINA_BAR_WIDTH);

    // Clamp fillWidth to ensure it stays between 0 and STAMINA_BAR_WIDTH 
    if (fillWidth < 0) fillWidth = 0;
    if (fillWidth > STAMINA_BAR_WIDTH) fillWidth = STAMINA_BAR_WIDTH;

    //  Draw Dark Gray background (empty portion) 
    DrawRectangle(STAMINA_BAR_X, STAMINA_BAR_Y, 
                  STAMINA_BAR_WIDTH, STAMINA_BAR_HEIGHT, DARKGRAY);

    //  Draw Blue dynamic fill bar 
    DrawRectangle(STAMINA_BAR_X, STAMINA_BAR_Y, 
                  fillWidth, STAMINA_BAR_HEIGHT, BLUE);

    //  Draw 1px White border outline 
    DrawRectangleLines(STAMINA_BAR_X, STAMINA_BAR_Y, 
                       STAMINA_BAR_WIDTH, STAMINA_BAR_HEIGHT, WHITE);

    //  Draw Stamina Text directly below the bar
    char textBuf[64];
    snprintf(textBuf, sizeof(textBuf), "Stamina: %.1f / %.1f", player->stamina, PLAYER_MAX_STAMINA);
    DrawText(textBuf, STAMINA_BAR_X, STAMINA_BAR_Y + STAMINA_BAR_HEIGHT + 5, 18, WHITE);
}

int main(void)
{
    
    const float groundY = (float)((MAP_ROWS * 4 / 5) * TILE_SIZE);   /* = 210 */  // added this

    // const int   minerSpeed = 5;
    // const float gravity    = 0.5f;
    // const float jumpForce  = -12.0f;
    // const float groundY    = screenHeight - 80.0f;  // top edge of the floor

    InitWindow(SCREEN_W, SCREEN_H, "Montana Tech Miner");  // changed

    GameMap map; 
    MapInit(&map);


    Player player = {
            .position = { (float)(SCREEN_W / 2 - PLAYER_W / 2), groundY - (float)PLAYER_H },
            .velocity = { 0.0f, 0.0f },
            .stamina = PLAYER_MAX_STAMINA,
            .onGround = true,
            .facingRight = true
        };

    
  
    SetTargetFPS(60);

    // Main game loop
    while (!WindowShouldClose())

    {
        // calculate stamina penalties 
        bool lowStamina = (player.stamina < JUMP_STAMINA_COST);
        float effectiveSpeed = BASE_SPD * (lowStamina ? 0.75f : 1.0f);
        float effectiveJump  = BASE_JUMP * (lowStamina ? 0.50f : 1.0f);

        // ── INPUT: changed ──────────────────────────────────────────────────────────
        if (IsKeyDown(KEY_RIGHT)) {
                player.velocity.x = effectiveSpeed;
                player.facingRight = true;
            } else if (IsKeyDown(KEY_LEFT)) {
                player.velocity.x = -effectiveSpeed;
                player.facingRight = false;
            } else {
                player.velocity.x = 0.0f;
            }

        if (IsKeyPressed(KEY_SPACE) && player.onGround) {
                    // Check if player has enough stamina for a full jump
                    bool hasFullStamina = (player.stamina >= JUMP_STAMINA_COST);
                    
                    // Apply jump velocity (effectiveJump will be half if stamina is low)
                    player.velocity.y = effectiveJump;
                    player.onGround = false;

                    // Only drain stamina if they had enough to pay the cost; otherwise, jump is free (half height)
                    if (hasFullStamina) {
                        UpdatePlayerStamina(&player, true);
                    } else {
                        // If stamina is already empty, keep it at 0
                        player.stamina = 0.0f;
                    }
                }

        // ── PHYSICS: changed ────────────────────────────────────────────────────────
        player.velocity.y += GRAVITY;
            if (player.velocity.y > MAX_FALL) player.velocity.y = MAX_FALL;

            player.position.x += player.velocity.x;
            player.position.y += player.velocity.y;
        
            // ── FLOOR COLLISION: changed ────────────────────────────────────────────────
        if (player.position.y + PLAYER_H >= groundY) {
                player.position.y = groundY - (float)PLAYER_H;
                player.velocity.y = 0.0f;
                player.onGround = true;
            }
        RegenerateStaminaFromFood(&player, &map);

        // ── SCREEN BOUNDARIES: changed ──────────────────────────────────────────────
        if (player.position.x < 0) 
                    player.position.x = 0.0f;
                if (player.position.x + PLAYER_W > (float)SCREEN_W) 
                    player.position.x = (float)(SCREEN_W - PLAYER_W);


        // ── ANIMATION ──────────────────────────────────────────────────────



        // ── DRAW: changed ───────────────────────────────────────────────────────────
        BeginDrawing();

            ClearBackground((Color){ 18, 10, 5, 255 });

            MapDraw(&map);

            /* player placeholder – green = facing right, lime = facing left */
            DrawRectangle((int)player.position.x, (int)player.position.y,
                    PLAYER_W, PLAYER_H,
                    player.facingRight ? LIME : GREEN);

            DrawStaminaBar(&player);
        EndDrawing();
    }


    CloseWindow();
    return 0;
}