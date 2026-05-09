/*
 * EREBUS — Death Card / Run Testimony System
 *
 * When the player dies, the game generates a "death card" showing
 * what the neural network learned that run: behavioral statistics,
 * how it caught you, and what weights changed the most.
 *
 * This data persists between runs — the monster remembers.
 */
#ifndef GAME_DEATHCARD_H
#define GAME_DEATHCARD_H

#include "ai/neural_net.h"
#include "game/player.h"

#define MAX_RUN_HISTORY 50

typedef enum {
    DEATH_CAUGHT,       /* Monster caught you                  */
    DEATH_TIMER,        /* Ran out of time                     */
    DEATH_SURVIVED,     /* Escaped (collected all cores + exit) */
} DeathReason;

/* Stats recorded each run for the neural network to learn from */
typedef struct {
    int         runNumber;
    DeathReason reason;
    float       survivalTime;
    float       totalDistance;
    float       timeSprinting;
    float       timeHiding;
    float       timeMoving;
    float       avgPanic;
    int         coresCollected;
    int         totalCores;
    float       monsterDistAtDeath;
} RunRecord;

/* Persistent player profile across all runs */
typedef struct {
    RunRecord runs[MAX_RUN_HISTORY];
    int       runCount;
    int       totalDeaths;
    int       totalEscapes;
    float     bestTime;
    float     worstTime;
} PlayerProfile;

/* ── API ──────────────────────────────────────────────────────────── */

void profile_init(PlayerProfile *prof);

/* Record the results of a run */
void profile_record_run(PlayerProfile *prof, const RunRecord *record);

/* Save/Load profile (for between-session persistence) */
bool profile_save(const PlayerProfile *prof, const char *filename);
bool profile_load(PlayerProfile *prof, const char *filename);

/* Generate a 16-character hex hash based on player profile stats */
void profile_get_fingerprint(const PlayerProfile *prof, char *outBuffer);

/* Generate training data from run history for the neural network */
void profile_get_training_bias(const PlayerProfile *prof, float biases[NN_OUTPUTS]);

/* ── Testimony System ────────────────────────────────────────────────── */

struct ReplayBuffer;

typedef struct {
    int   phase;          /* 0 = Init, 1 = Question, 2 = Concluding */
    int   questionId;     /* Which question is asked */
    int   selectedAnswer; /* Currently highlighted answer (0, 1, 2) */
    float timer;          /* Animation/delay timer */
    bool  done;           /* Is testimony finished? */
} TestimonyState;

void testimony_init(TestimonyState *t);
/* Returns true when the testimony phase is completely finished */
bool testimony_update_and_draw(TestimonyState *t, struct ReplayBuffer *rb, float dt);

#endif /* GAME_DEATHCARD_H */
