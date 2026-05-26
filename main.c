//////////////////////////////////////////////////////////////
//                                                          //
//     Made With love by Anes Diaf https://anes.is-a.dev    //
//                                                          //
//          Refactored & bug-fixed for correctness          //
//                                                          //
//////////////////////////////////////////////////////////////

#include "raylib.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define SCREEN_WIDTH        400
#define SCREEN_HEIGHT       800
#define PLAYER_SPEED        600         // Pixels per second
#define PROJECTILE_SPEED    500         // Pixels per second (upward)
#define MAX_ENEMIES         5           // Maximum live enemies at once
#define ENEMY_SPEED         60          // Pixels per second (downward)
#define MAX_PROJECTILES     4           // Maximum live projectiles at once
#define PLAYER_SCALE        1.5f        // Draw scale applied to the player sprite
#define SHOOT_COOLDOWN      0.2f        // Minimum seconds between shots
#define GAME_SCALE      1.0f            // Draw scale applied to all sprites

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

// Represents a single enemy ship on screen.
typedef struct {
    Vector2   position;       // Top-left world position
    Rectangle collisionBox;   // Axis-aligned bounding box used for hit detection
} Enemy;

// Represents a single player-fired projectile.
typedef struct {
    Vector2   position;       // Top-left world position
    Rectangle collisionBox;   // AABB used for hit detection
    bool      active;         // True while the projectile is in flight
} Projectile;

// ---------------------------------------------------------------------------
// Helper: reset an enemy to a random position above the screen
// ---------------------------------------------------------------------------
static void ResetEnemy(Enemy *e, int spriteWidth, int spriteHeight)
{
    float spawnX = (float)GetRandomValue(0, SCREEN_WIDTH - spriteWidth);
    e->position    = (Vector2){ spawnX, -17.0f };
    e->collisionBox = (Rectangle){ spawnX, -17.0f,
                                   (float)spriteWidth,
                                   (float)spriteHeight };
}

// ---------------------------------------------------------------------------
// Helper: spawn a new projectile from the player's gun
// ---------------------------------------------------------------------------
static void SpawnProjectile(Projectile projectiles[], int *projCount,
                            Vector2 playerPosition, int spriteWidth)
{
    // Ring-buffer: recycle oldest slot when the pool is full.
    if (*projCount >= MAX_PROJECTILES) *projCount = 0;

    int slot = *projCount;

    float px = playerPosition.x + (spriteWidth * PLAYER_SCALE) / 2.0f - 1.5f;
    float py = playerPosition.y;

    projectiles[slot].position    = (Vector2){ px, py };
    projectiles[slot].collisionBox = (Rectangle){ px, py, 3.0f, 8.0f };
    projectiles[slot].active      = true;

    (*projCount)++;   // Increment exactly once — SpawnProjectile owns this.
}

// ---------------------------------------------------------------------------
// Helper: draw the player sprite and keep its collision box in sync
// ---------------------------------------------------------------------------
static void DrawPlayer(Texture2D sprite, Vector2 position,
                       Rectangle *collisionBox)
{
    collisionBox->x = position.x;
    collisionBox->y = position.y;

    // Set alpha > 0 here to visualise the hitbox while debugging.
    DrawRectangleRec(*collisionBox, (Color){ 255, 0, 0, 0 });
    DrawTextureEx(sprite, position, 0.0f, PLAYER_SCALE, WHITE);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(void)
{
    // -----------------------------------------------------------------------
    // Window setup
    // -----------------------------------------------------------------------
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "DFND");
    SetTargetFPS(60);

    // -----------------------------------------------------------------------
    // Asset loading
    // -----------------------------------------------------------------------
    Texture2D playerSprite     = LoadTexture("assets/main-ship.png");
    Texture2D enemySprite      = LoadTexture("assets/enemy-ship.png");
    Texture2D projectileSprite = LoadTexture("assets/default-projectile.png");

    // -----------------------------------------------------------------------
    // Player state
    // -----------------------------------------------------------------------
    Vector2 playerPosition = {
        SCREEN_WIDTH / 2.0f - (playerSprite.width * PLAYER_SCALE) / 2.0f,
        SCREEN_HEIGHT - 60.0f
    };

    // Collision box dimensions derived from the player sprite + scale.
    Rectangle playerCollisionRect = {
        playerPosition.x,
        playerPosition.y,
        playerSprite.width  * PLAYER_SCALE,
        playerSprite.height * PLAYER_SCALE
    };

    float shootTimer = 0.0f;   // Tracks time since last shot (cooldown)

    // -----------------------------------------------------------------------
    // Projectile pool
    // -----------------------------------------------------------------------
    Projectile projectiles[MAX_PROJECTILES] = { 0 };
    int projCount = 0;

    // -----------------------------------------------------------------------
    // Enemy pool  — spawn all enemies above the screen at the start
    // -----------------------------------------------------------------------
    Enemy enemies[MAX_ENEMIES] = { 0 };
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        ResetEnemy(&enemies[i], enemySprite.width, enemySprite.height);
        // Stagger starting Y positions so they don't all arrive at once.
        enemies[i].position.y       -= (float)(i * 200);
        enemies[i].collisionBox.y   -= (float)(i * 200);
    }

    // -----------------------------------------------------------------------
    // Game state
    // -----------------------------------------------------------------------
    bool isMenu  = true;
    bool gameOver = false;
    int  score   = 0;

    // -----------------------------------------------------------------------
    // Main game loop
    // -----------------------------------------------------------------------
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        // -------------------------------------------------------------------
        // MENU screen
        // -------------------------------------------------------------------
        if (isMenu)
        {
            if (IsKeyPressed(KEY_ENTER)) isMenu = false;

            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("DFND",
                     SCREEN_WIDTH / 2 - MeasureText("DFND", 48) / 2,
                     SCREEN_HEIGHT / 2 - 80, 48, WHITE);
            DrawText("Press ENTER to start",
                     SCREEN_WIDTH / 2 - MeasureText("Press ENTER to start", 20) / 2,
                     SCREEN_HEIGHT / 2, 20, GRAY);
            DrawText("Arrow keys to move  |  SPACE to fire",
                     SCREEN_WIDTH / 2 - MeasureText("Arrow keys to move  |  SPACE to fire", 16) / 2,
                     SCREEN_HEIGHT / 2 + 40, 16, DARKGRAY);
            DrawText("Made by Anes Diaf",
                     SCREEN_WIDTH / 2 - MeasureText("Made by Anes Diaf", 20) / 2,
                     SCREEN_HEIGHT - 80, 20, GRAY);
            EndDrawing();
            continue;   // Skip all gameplay logic while in the menu.
        }

        // -------------------------------------------------------------------
        // GAME OVER screen
        // -------------------------------------------------------------------
        if (gameOver)
        {
            if (IsKeyPressed(KEY_ENTER))
            {
                // Reset everything for a new run.
                score     = 0;
                projCount = 0;
                gameOver  = false;
                for (int i = 0; i < MAX_PROJECTILES; i++) projectiles[i].active = false;
                for (int i = 0; i < MAX_ENEMIES; i++)
                {
                    ResetEnemy(&enemies[i], enemySprite.width, enemySprite.height);
                    enemies[i].position.y     -= (float)(i * 200);
                    enemies[i].collisionBox.y -= (float)(i * 200);
                }
                playerPosition = (Vector2){
                    SCREEN_WIDTH / 2.0f - (playerSprite.width * PLAYER_SCALE) / 2.0f,
                    SCREEN_HEIGHT - 60.0f
                };
            }

            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("GAME OVER",
                     SCREEN_WIDTH / 2 - MeasureText("GAME OVER", 48) / 2,
                     SCREEN_HEIGHT / 2 - 60, 48, RED);
            DrawText(TextFormat("Score: %d", score),
                     SCREEN_WIDTH / 2 - MeasureText(TextFormat("Score: %d", score), 28) / 2,
                     SCREEN_HEIGHT / 2 + 10, 28, WHITE);
            DrawText("Press ENTER to retry",
                     SCREEN_WIDTH / 2 - MeasureText("Press ENTER to retry", 20) / 2,
                     SCREEN_HEIGHT / 2 + 60, 20, GRAY);
            EndDrawing();
            continue;
        }

        // -------------------------------------------------------------------
        // GAMEPLAY — Input
        // -------------------------------------------------------------------
        if (IsKeyDown(KEY_LEFT) && playerPosition.x > 0.0f)
            playerPosition.x -= PLAYER_SPEED * delta;

        if (IsKeyDown(KEY_RIGHT) &&
            playerPosition.x < SCREEN_WIDTH - playerSprite.width * PLAYER_SCALE)
            playerPosition.x += PLAYER_SPEED * delta;

        // Shoot with cooldown so SPACE held down doesn't spam bullets.
        shootTimer += delta;
        if (IsKeyDown(KEY_SPACE) && shootTimer >= SHOOT_COOLDOWN)
        {
            SpawnProjectile(projectiles, &projCount,
                            playerPosition, playerSprite.width);
            shootTimer = 0.0f;
        }

        // -------------------------------------------------------------------
        // GAMEPLAY — Update projectiles
        // -------------------------------------------------------------------
        for (int i = 0; i < MAX_PROJECTILES; i++)
        {
            if (!projectiles[i].active) continue;

            // Move upward.
            float movement = PROJECTILE_SPEED * delta;
            projectiles[i].position.y    -= movement;
            projectiles[i].collisionBox.y -= movement;

            // Deactivate when it leaves the top of the screen.
            if (projectiles[i].position.y + projectiles[i].collisionBox.height < 0.0f)
                projectiles[i].active = false;
        }

        // -------------------------------------------------------------------
        // GAMEPLAY — Update enemies
        // -------------------------------------------------------------------
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            bool hitByProjectile = false;

            // Check collision with every active projectile.
            for (int j = 0; j < MAX_PROJECTILES; j++)
            {
                if (!projectiles[j].active) continue;
                if (CheckCollisionRecs(enemies[i].collisionBox,
                                       projectiles[j].collisionBox))
                {
                    projectiles[j].active = false;   // Consume the projectile.
                    hitByProjectile       = true;
                    score++;
                    break;   // One projectile can only kill one enemy.
                }
            }

            if (hitByProjectile)
            {
                // Respawn the enemy above the screen.
                ResetEnemy(&enemies[i], enemySprite.width, enemySprite.height);
            }
            else
            {
                // Move the enemy downward.
                enemies[i].position.y     += ENEMY_SPEED * delta;
                enemies[i].collisionBox.y  = enemies[i].position.y;

                // Check if the enemy has reached/passed the player — game over.
                if (enemies[i].position.y > SCREEN_HEIGHT)
                {
                    gameOver = true;
                }

                // Check direct collision with the player.
                if (CheckCollisionRecs(enemies[i].collisionBox, playerCollisionRect))
                {
                    gameOver = true;
                }
            }
        }

        // -------------------------------------------------------------------
        // DRAW
        // -------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);

        // HUD
        DrawText(TextFormat("Score: %d", score), 10, 10, 20, BLUE);

        // Player
        DrawPlayer(playerSprite, playerPosition, &playerCollisionRect);

        // Enemies
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            DrawTexture(enemySprite,
                        (int)enemies[i].position.x,
                        (int)enemies[i].position.y,
                        WHITE);
            // Uncomment to visualise enemy hitboxes:
            // DrawRectangleLinesEx(enemies[i].collisionBox, 1, RED);
        }

        // Projectiles
        for (int i = 0; i < MAX_PROJECTILES; i++)
        {
            if (!projectiles[i].active) continue;
            DrawTexture(projectileSprite,
                        (int)projectiles[i].position.x,
                        (int)projectiles[i].position.y,
                        WHITE);
        }

        EndDrawing();
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    UnloadTexture(projectileSprite);
    UnloadTexture(enemySprite);
    UnloadTexture(playerSprite);
    CloseWindow();

    return 0;
}
