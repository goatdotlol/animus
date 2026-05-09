/*
 * EREBUS — Monster AI
 */
#ifndef AI_MONSTER_H
#define AI_MONSTER_H

#include "ai/neural_net.h"
#include "ai/pathfinding.h"

typedef enum {
    MSTATE_IDLE,        /* Waiting / Not active */
    MSTATE_WANDER,      /* Randomly wandering the map */
    MSTATE_INVESTIGATE, /* Moving to a source of noise */
    MSTATE_CHASE        /* Direct line-of-sight chase */
} MonsterState;

typedef struct {
    float x, y;
    float dirX, dirY;
    float moveSpeed;

    MonsterState state;
    NeuralNet    brain;
    Path         currentPath;

    /* Sprite info for the renderer */
    int spriteId;       /* Registered with raycaster */
    
    /* Memory / Senses */
    float lastSeenPlayerX;
    float lastSeenPlayerY;
    float suspicion;    /* 0..1, fills up before chasing */
} Monster;

struct Map;
struct Player;
struct RaycastRenderer;

void monster_init(Monster *m, struct RaycastRenderer *r, float startX, float startY);

/* Main update loop for the monster */
void monster_update(Monster *m, const struct Map *map, const struct Player *p, float dt);

#endif /* AI_MONSTER_H */
