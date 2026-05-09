/*
 * EREBUS — Player Controller
 *
 * First-person movement with collision detection, four speed modes,
 * head bob, flashlight, and behavioral tracking for the neural network.
 */
#include "game/player.h"
#include "engine/raycaster.h"
#include "world/map.h"
#include <math.h>

/* Speed constants per mode */
static const float SPEED_SPRINT = 4.5f;
static const float SPEED_WALK   = 2.5f;
static const float SPEED_CROUCH = 1.2f;
static const float SPEED_FREEZE = 0.0f;

/* Noise radii per mode (in map cells) */
static const float NOISE_SPRINT = 8.0f;
static const float NOISE_WALK   = 4.0f;
static const float NOISE_CROUCH = 2.0f;
static const float NOISE_FREEZE = 0.0f;

static const float MOUSE_SENS   = 0.003f;
static const float BOB_SPEED    = 10.0f;
static const float BOB_AMOUNT   = 4.0f;
static const float PITCH_LIMIT  = 0.35f;
static const float COLLISION_R  = 0.2f;  /* Collision radius */

void player_init(Player *p, float x, float y, float angle) {
    p->posX = x;
    p->posY = y;
    p->angle = angle;

    /* Calculate direction and camera plane from angle */
    p->dirX = cosf(angle);
    p->dirY = sinf(angle);

    /* Camera plane perpendicular to direction, scaled for FOV */
    float planeMag = tanf(RC_FOV * 0.5f * (3.14159265f / 180.0f));
    p->planeX = -p->dirY * planeMag;
    p->planeY =  p->dirX * planeMag;

    p->pitch = 0.0f;
    p->moveMode = MOVE_WALK;
    p->moveSpeed = SPEED_WALK;
    p->bobTimer = 0.0f;
    p->bobAmount = 0.0f;
    p->isMoving = false;

    p->flashlightOn = true;
    p->flashlightBattery = 1.0f;

    p->timeSprinting = 0.0f;
    p->timeHiding = 0.0f;
    p->timeMoving = 0.0f;
    p->panicInput = 0.0f;
    p->noiseRadius = NOISE_WALK;
}

void player_set_mode(Player *p, MoveMode mode) {
    p->moveMode = mode;
    switch (mode) {
        case MOVE_SPRINT: p->moveSpeed = SPEED_SPRINT; p->noiseRadius = NOISE_SPRINT; break;
        case MOVE_WALK:   p->moveSpeed = SPEED_WALK;   p->noiseRadius = NOISE_WALK;   break;
        case MOVE_CROUCH: p->moveSpeed = SPEED_CROUCH; p->noiseRadius = NOISE_CROUCH; break;
        case MOVE_FREEZE: p->moveSpeed = SPEED_FREEZE; p->noiseRadius = NOISE_FREEZE; break;
    }
}

/* Try to move with collision detection */
static void try_move(Player *p, const Map *map, float dx, float dy) {
    float newX = p->posX + dx;
    float newY = p->posY + dy;

    /* Check X movement */
    if (map_is_walkable(map, newX + COLLISION_R * (dx > 0 ? 1 : -1), p->posY + COLLISION_R) &&
        map_is_walkable(map, newX + COLLISION_R * (dx > 0 ? 1 : -1), p->posY - COLLISION_R)) {
        p->posX = newX;
    }

    /* Check Y movement */
    if (map_is_walkable(map, p->posX + COLLISION_R, p->posY + dy + COLLISION_R * (dy > 0 ? 1 : -1)) &&
        map_is_walkable(map, p->posX - COLLISION_R, p->posY + dy + COLLISION_R * (dy > 0 ? 1 : -1))) {
        p->posY += dy;
    }
}

void player_update(Player *p, const Map *map, float dt) {
    /* ── Mouse look ────────────────────────────────────────────── */
    float mouseDX = GetMouseDelta().x;
    float mouseDY = GetMouseDelta().y;

    p->angle += mouseDX * MOUSE_SENS;

    /* Pitch (limited vertical look) */
    p->pitch -= mouseDY * MOUSE_SENS * 0.8f;
    if (p->pitch >  PITCH_LIMIT) p->pitch =  PITCH_LIMIT;
    if (p->pitch < -PITCH_LIMIT) p->pitch = -PITCH_LIMIT;

    /* Update direction vector from angle */
    p->dirX = cosf(p->angle);
    p->dirY = sinf(p->angle);

    float planeMag = tanf(RC_FOV * 0.5f * (3.14159265f / 180.0f));
    p->planeX = -p->dirY * planeMag;
    p->planeY =  p->dirX * planeMag;

    /* ── Movement mode selection ───────────────────────────────── */
    if (IsKeyDown(KEY_LEFT_SHIFT))       player_set_mode(p, MOVE_SPRINT);
    else if (IsKeyDown(KEY_LEFT_CONTROL)) player_set_mode(p, MOVE_CROUCH);
    else if (IsKeyDown(KEY_SPACE))        player_set_mode(p, MOVE_FREEZE);
    else                                  player_set_mode(p, MOVE_WALK);

    /* ── WASD movement ─────────────────────────────────────────── */
    float moveX = 0, moveY = 0;
    p->isMoving = false;

    if (IsKeyDown(KEY_W)) { moveX += p->dirX; moveY += p->dirY; p->isMoving = true; }
    if (IsKeyDown(KEY_S)) { moveX -= p->dirX; moveY -= p->dirY; p->isMoving = true; }
    if (IsKeyDown(KEY_A)) { moveX += p->dirY; moveY -= p->dirX; p->isMoving = true; }
    if (IsKeyDown(KEY_D)) { moveX -= p->dirY; moveY += p->dirX; p->isMoving = true; }

    /* Normalize diagonal movement */
    float moveLen = sqrtf(moveX * moveX + moveY * moveY);
    if (moveLen > 0.01f) {
        moveX /= moveLen;
        moveY /= moveLen;
    }

    try_move(p, map, moveX * p->moveSpeed * dt, moveY * p->moveSpeed * dt);

    /* ── Head bob ──────────────────────────────────────────────── */
    if (p->isMoving && p->moveMode != MOVE_FREEZE) {
        float bobFreq = (p->moveMode == MOVE_SPRINT) ? BOB_SPEED * 1.5f :
                        (p->moveMode == MOVE_CROUCH) ? BOB_SPEED * 0.5f : BOB_SPEED;
        p->bobTimer += dt * bobFreq;
        float bobMag = (p->moveMode == MOVE_SPRINT) ? BOB_AMOUNT * 1.5f :
                       (p->moveMode == MOVE_CROUCH) ? BOB_AMOUNT * 0.3f : BOB_AMOUNT;
        p->bobAmount = sinf(p->bobTimer) * bobMag;
    } else {
        /* Smoothly return to center */
        p->bobAmount *= 0.9f;
        p->bobTimer = 0;
    }

    /* ── Flashlight toggle ─────────────────────────────────────── */
    if (IsKeyPressed(KEY_F)) {
        p->flashlightOn = !p->flashlightOn;
    }
    if (p->flashlightOn) {
        p->flashlightBattery -= dt * 0.01f;
        if (p->flashlightBattery <= 0) {
            p->flashlightBattery = 0;
            p->flashlightOn = false;
        }
    }

    /* ── Behavioral tracking (for neural network) ──────────────── */
    if (p->moveMode == MOVE_SPRINT) p->timeSprinting += dt;
    if (p->moveMode == MOVE_FREEZE || !p->isMoving) p->timeHiding += dt;
    if (p->isMoving) p->timeMoving += dt;

    /* Panic score: high mouse movement + rapid input changes */
    float inputChange = fabsf(mouseDX) + fabsf(mouseDY);
    p->panicInput = p->panicInput * 0.95f + inputChange * 0.05f;
}
