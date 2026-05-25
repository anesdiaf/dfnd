//////////////////////////////////////////////////////////////
//                                                          //
//     Made With love by Anes Diaf https://anes.is-a.dev    //
//                                                          //
//          Refactored & commented for readability          //
//                                                          //
//                                                          //
//////////////////////////////////////////////////////////////

#include "raylib.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define SCREEN_WIDTH        400
#define SCREEN_HEIGHT       800
#define PLAYER_SPEED        600          // Pixels per second
#define PROJECTILE_SPEED    300          // Pixels per second (upward)
#define ENEMY_SPEED         60           // Pixels per second (downward)
#define MAX_PROJECTILES     6            // Maximum live projectiles at once
#define PLAYER_SCALE        1.5f         // Draw scale applied to the player sprite

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

// Represents a single enemy ship on screen.
typedef struct {
    Vector2   position;       // Top-left world position
    float     speed;          // Movement speed (currently unused; uses ENEMY_SPEED)
} Enemy;

// Represents a single player-fired projectile.
typedef struct {
    Vector2   position;       // Top-left world position
    Rectangle collisionBox;   // Axis-aligned bounding box used for hit detection
    bool      isDone;         // True once the projectile has hit something or expired
} Projectile;

// ---------------------------------------------------------------------------
// Helper: draw the player sprite and keep its collision box in sync
// ---------------------------------------------------------------------------
static void DrawPlayer(Texture2D sprite, Vector2 position, Rectangle *collisionBox)
{
    // Keep the collision rectangle aligned with the player's current position.
    collisionBox->x = position.x;
    collisionBox->y = position.y;

    // Draw the collision box (alpha = 0 makes it invisible in release builds;
    // change the alpha value to 128+ while debugging to see it).
    DrawRectangleRec(*collisionBox, (Color){ 255, 0, 0, 0 });

    // Draw the player sprite at the scaled size.
    DrawTextureEx(sprite, position, 0.0f, PLAYER_SCALE, WHITE);
}

// ---------------------------------------------------------------------------
// Helper: draw an enemy sprite and keep its collision box in sync
// ---------------------------------------------------------------------------
static void DrawEnemy(Texture2D sprite, Vector2 position, Rectangle *collisionBox)
{
    // Keep the collision rectangle aligned with the enemy's current position.
    collisionBox->x = position.x;
    collisionBox->y = position.y;

    // Draw the collision box (visible blue rectangle — useful for debugging).
    DrawRectangleRec(*collisionBox, BLUE);

    // Draw the enemy sprite at 1:1 scale.
    DrawTexture(sprite, (int)position.x, (int)position.y, WHITE);
}

// ---------------------------------------------------------------------------
// Helper: spawn a new projectile originating from the player's gun position
// ---------------------------------------------------------------------------
static void SpawnProjectile(Projectile projectiles[], int *projCount,
                            Vector2 playerPosition, int spriteWidth)
{
    // Cycle the projectile slot back to 0 when the array is full.
    // This recycles the oldest projectile (simple ring-buffer approach).
    if (*projCount >= MAX_PROJECTILES) {
        *projCount = 0;
    }

    int slot = *projCount;

    // Centre the projectile horizontally on the player sprite.
    projectiles[slot].position.x = playerPosition.x + (spriteWidth / 2.0f);
    projectiles[slot].position.y = playerPosition.y;

    // Initialise the collision box to match the starting position.
    projectiles[slot].collisionBox = (Rectangle){
        projectiles[slot].position.x,
        projectiles[slot].position.y,
        3.0f,   // Width  (thin bullet)
        2.0f    // Height (thin bullet)
    };

    projectiles[slot].isDone = false;

    (*projCount)++;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(void)
{
    // -----------------------------------------------------------------------
    // Window & renderer setup
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
    // Start the player centred horizontally near the bottom of the screen.
    Vector2 playerPosition = {
        SCREEN_WIDTH / 2.0f,
        SCREEN_HEIGHT - 40.0f
    };

    // Collision box for the player (width/height match the base sprite size).
    Rectangle playerCollisionRect = { 0.0f, 0.0f, 24.0f, 24.0f };

    // -----------------------------------------------------------------------
    // Enemy state
    // -----------------------------------------------------------------------
    // Pick a random horizontal spawn position that keeps the sprite on screen.
    float spawnX = (float)GetRandomValue(enemySprite.width,
                                         SCREEN_WIDTH - enemySprite.width);
    Vector2 enemyPosition = { spawnX, -17.0f };   // Start just above the visible area

    // Collision box for the enemy (offset slightly so it hugs the sprite).
    Rectangle enemyCollisionRect = { 0.0f, 50.0f, 16.0f, 16.0f };

    // -----------------------------------------------------------------------
    // Projectile pool
    // -----------------------------------------------------------------------
    Projectile projectiles[MAX_PROJECTILES] = { 0 };
    int projCount = 0;   // Index of the next available slot

    // -----------------------------------------------------------------------
    // Game state
    // -----------------------------------------------------------------------
    int  score          = 0;
    bool enemyCollision = false;   // True while the enemy overlaps the player

    // -----------------------------------------------------------------------
    // Main game loop
    // -----------------------------------------------------------------------
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();   // Seconds elapsed since the last frame

        // -------------------------------------------------------------------
        // INPUT — Player horizontal movement
        // Multiply delta by PLAYER_SPEED for frame-rate-independent movement.
        // The boundary checks prevent the player from leaving the screen.
        // -------------------------------------------------------------------
        if (IsKeyDown(KEY_LEFT) && playerPosition.x > 0.0f)
        {
            playerPosition.x -= PLAYER_SPEED * delta;
        }
        if (IsKeyDown(KEY_RIGHT) &&
            playerPosition.x < SCREEN_WIDTH - (playerSprite.width * PLAYER_SCALE))
        {
            playerPosition.x += PLAYER_SPEED * delta;
        }

        // -------------------------------------------------------------------
        // INPUT — Fire projectile on SPACE (one shot per key press)
        // -------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE))
        {
            SpawnProjectile(projectiles, &projCount,
                            playerPosition, playerSprite.width);
        }

        // -------------------------------------------------------------------
        // PHYSICS — Move the enemy downward (pause when colliding with player)
        // -------------------------------------------------------------------
        if (!enemyCollision)
        {
            enemyPosition.y += ENEMY_SPEED * delta;
        }

        // -------------------------------------------------------------------
        // COLLISION — Player vs. Enemy
        // -------------------------------------------------------------------
        enemyCollision = CheckCollisionRecs(playerCollisionRect, enemyCollisionRect);

        // -------------------------------------------------------------------
        // DRAW
        // -------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);

        // HUD
        DrawText(TextFormat("Score: %02i", score), 10, 10, 16, BLUE);

        // Player
        DrawPlayer(playerSprite, playerPosition, &playerCollisionRect);

        // -------------------------------------------------------------------
        // Projectile update & draw loop
        // -------------------------------------------------------------------
        for (int i = 0; i < MAX_PROJECTILES; i++)
        {
            // Skip slots that have already finished (hit or recycled).
            if (projectiles[i].isDone) continue;

            // Check whether this projectile has hit the enemy this frame.
            bool hitEnemy = CheckCollisionRecs(projectiles[i].collisionBox,
                                               enemyCollisionRect);

            if (hitEnemy)
            {
                // Award a point and mark the projectile as consumed.
                score++;
                projectiles[i].isDone = true;

                // Respawn the enemy at a new random horizontal position,
                // just above the top of the screen.
                float newX = (float)GetRandomValue(enemySprite.width,
                                                   SCREEN_WIDTH - enemySprite.width);
                enemyPosition = (Vector2){ newX, -17.0f };
            }
            else
            {
                // Move the projectile upward.
                float movement = PROJECTILE_SPEED * delta;
                projectiles[i].position.y    -= movement;
                projectiles[i].collisionBox.y -= movement;

                // Draw the projectile sprite.
                DrawTexture(projectileSprite,
                            (int)projectiles[i].position.x,
                            (int)projectiles[i].position.y,
                            WHITE);

                // Draw the collision box (visible red rect — set alpha to 0 to hide).
                DrawRectangleRec(projectiles[i].collisionBox, RED);
            }
        }

        // Enemy (drawn after projectiles so it appears on top)
        DrawEnemy(enemySprite, enemyPosition, &enemyCollisionRect);

        EndDrawing();
    }

    // -----------------------------------------------------------------------
    // Cleanup — unload GPU resources before closing
    // -----------------------------------------------------------------------
    UnloadTexture(projectileSprite);
    UnloadTexture(enemySprite);
    UnloadTexture(playerSprite);
    CloseWindow();

    return 0;
}
