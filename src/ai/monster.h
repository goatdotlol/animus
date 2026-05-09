/*
 * EREBUS — Monster AI (v2: Advanced Behaviors)
 */
#ifndef AI_MONSTER_H
#define AI_MONSTER_H

#include "ai/neural_net.h"
#include "ai/pathfinding.h"

typedef enum {
    MSTATE_IDLE,        /* Waiting / Not active */
    MSTATE_WANDER,      /* Randomly wandering the map */
    MSTATE_INVESTIGATE, /* Moving to a source of noise */
    MSTATE_CHASE,       /* Direct line-of-sight chase */
    MSTATE_STALK,       /* Sees player but holds back (menace) */
    MSTATE_GLITCH       /* Overfitting detected — erratic behavior */
} MonsterState;

/* Menace Gauge — controls dramatic tension */
typedef struct {
    float tension;       /* 0..1 current tension level */
    float mercyTimer;    /* Cooldown: how long to fake mercy */
    int   nearMissCount; /* Times monster was close but "lost" player */
    bool  isFakingMercy; /* Currently pretending to lose the player */
    float dramaPause;    /* Pause before the kill for horror effect */
} MenaceGauge;

typedef struct {
    float x, y;
    float dirX, dirY;
    float moveSpeed;

    MonsterState state;
    NeuralNet    brain;
    Path         currentPath;

    /* Sprite info for the renderer */
    int spriteId;

    /* Memory / Senses */
    float lastSeenPlayerX;
    float lastSeenPlayerY;
    float suspicion;    /* 0..1, fills up before chasing */

    /* Menace system */
    MenaceGauge menace;

    /* Environmental manipulation */
    float envTimer;      /* Cooldown for environment actions */
    int   lightsKilled;  /* Number of lights turned off this run */

    /* Overfitting detection */
    float patternScore;   /* How repetitive the monster's behavior is */
    float lastOutputs[8]; /* Previous NN outputs for comparison */
    int   sameOutputCount;/* Consecutive similar outputs */
    float glitchTimer;    /* Timer for glitch state duration */
} Monster;

struct Map;
struct Player;
struct RaycastRenderer;

void monster_init(Monster *m, struct RaycastRenderer *r, float startX, float startY, int runNumber);
void monster_update(Monster *m, struct Map *map, const struct Player *p, float dt);

#endif /* AI_MONSTER_H */
