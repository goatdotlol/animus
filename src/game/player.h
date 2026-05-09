/*
 * EREBUS — Player Controller
 * First-person movement with 4 speed modes, collision detection,
 * head bob, flashlight, and noise footprint system.
 */
#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include "raylib.h"

/* ── Movement Modes ───────────────────────────────────────────────── */
typedef enum {
    MOVE_SPRINT,    /* Fast + loud  (noise radius 8 sectors)  */
    MOVE_WALK,      /* Normal       (noise radius 4 sectors)  */
    MOVE_CROUCH,    /* Slow + quiet (noise radius 2 sectors)  */
    MOVE_FREEZE     /* Stationary   (noise radius 0)          */
} MoveMode;

/* ── Player State ─────────────────────────────────────────────────── */
typedef struct Player {
    /* Position and orientation */
    float posX, posY;       /* World position                        */
    float dirX, dirY;       /* Direction vector (normalized)          */
    float planeX, planeY;   /* Camera plane (perpendicular to dir)    */
    float angle;            /* Facing angle in radians                */
    float pitch;            /* Vertical look offset (-0.3..0.3)       */

    /* Movement */
    MoveMode moveMode;
    float    moveSpeed;     /* Current effective speed                 */
    float    bobTimer;      /* Head bob oscillation timer              */
    float    bobAmount;     /* Current head bob offset                 */
    bool     isMoving;

    /* Flashlight */
    bool  flashlightOn;
    float flashlightBattery; /* 0..1 */

    /* Stats for neural network input */
    float timeSprinting;
    float timeHiding;
    float timeMoving;
    float panicInput;       /* Erratic input frequency score          */

    /* Noise system */
    float noiseRadius;      /* Current noise emission radius in cells */
} Player;

/* ── Public API ───────────────────────────────────────────────────── */
struct Map;

void player_init(Player *p, float x, float y, float angle);
void player_update(Player *p, const struct Map *map, float dt);
void player_set_mode(Player *p, MoveMode mode);

#endif /* GAME_PLAYER_H */
