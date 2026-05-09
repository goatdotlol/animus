/*
 * EREBUS — Replay Buffer & Training System
 *
 * Records snapshots of game state during play, then uses them
 * to train the monster's neural network between runs via backpropagation.
 * This is how the monster ACTUALLY learns your behavior.
 */
#ifndef AI_TRAINING_H
#define AI_TRAINING_H

#include "ai/neural_net.h"
#include "game/deathcard.h"

#define REPLAY_MAX_SAMPLES 256
#define TRAINING_EPOCHS    20
#define LEARNING_RATE      0.01f
#define MUTATION_RATE      0.05f

/* A single snapshot of game state during play */
typedef struct {
    float inputs[NN_INPUTS];   /* What the monster saw        */
    float outputs[NN_OUTPUTS]; /* What it decided to do       */
    float reward;              /* How good that decision was   */
} ReplaySample;

/* The replay buffer collects samples during a run */
typedef struct {
    ReplaySample samples[REPLAY_MAX_SAMPLES];
    int          count;
    int          writeHead;
} ReplayBuffer;

/* ── API ──────────────────────────────────────────────────────────── */

void replay_init(ReplayBuffer *rb);

/* Record a sample during gameplay */
void replay_record(ReplayBuffer *rb, const float inputs[NN_INPUTS],
                   const float outputs[NN_OUTPUTS], float reward);

/* Train the neural network using collected replay data.
 * Called between runs. Returns average loss. */
float training_run(NeuralNet *nn, ReplayBuffer *rb, const PlayerProfile *prof);

/* Apply NEAT-style weight mutation after a run.
 * Mutation intensity scales with how many times the player escaped. */
void training_mutate(NeuralNet *nn, const PlayerProfile *prof, unsigned int seed);

#endif /* AI_TRAINING_H */
