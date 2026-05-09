/*
 * EREBUS — Replay Buffer & Training Implementation
 *
 * The actual learning algorithm. Between runs:
 * 1. Analyze replay buffer (what the monster saw vs what happened)
 * 2. Compute reward signals (closer to player = good, player escaped = bad)
 * 3. Backpropagate through the network to adjust weights
 * 4. Apply NEAT-style mutations to prevent local optima
 */
#include "ai/training.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

void replay_init(ReplayBuffer *rb) {
    memset(rb, 0, sizeof(*rb));
}

void replay_record(ReplayBuffer *rb, const float inputs[NN_INPUTS],
                   const float outputs[NN_OUTPUTS], float reward) {
    int idx = rb->writeHead;
    memcpy(rb->samples[idx].inputs, inputs, sizeof(float) * NN_INPUTS);
    memcpy(rb->samples[idx].outputs, outputs, sizeof(float) * NN_OUTPUTS);
    rb->samples[idx].reward = reward;

    rb->writeHead = (rb->writeHead + 1) % REPLAY_MAX_SAMPLES;
    if (rb->count < REPLAY_MAX_SAMPLES) rb->count++;
}

/* ── Backpropagation ──────────────────────────────────────────────── */

static float relu_deriv(float x) { return x > 0.0f ? 1.0f : 0.0f; }
static float sigmoid_deriv(float x) { return x * (1.0f - x); }

/* Train on a single sample. Returns squared error. */
static float train_sample(NeuralNet *nn, const float inputs[NN_INPUTS],
                          const float targets[NN_OUTPUTS], float lr) {
    float outputs[NN_OUTPUTS];
    nn_forward(nn, inputs, outputs);

    /* ── Output layer error ───────────────────────────────────────── */
    float d3[NN_OUTPUTS];
    float totalError = 0;
    for (int j = 0; j < NN_OUTPUTS; j++) {
        float err = targets[j] - nn->a3[j];
        d3[j] = err * sigmoid_deriv(nn->a3[j]);
        totalError += err * err;
    }

    /* ── Hidden layer 2 error ─────────────────────────────────────── */
    float d2[NN_HIDDEN2];
    for (int j = 0; j < NN_HIDDEN2; j++) {
        float err = 0;
        for (int k = 0; k < NN_OUTPUTS; k++) {
            err += d3[k] * nn->w3[j][k];
        }
        d2[j] = err * relu_deriv(nn->a2[j]);
    }

    /* ── Hidden layer 1 error ─────────────────────────────────────── */
    float d1[NN_HIDDEN1];
    for (int j = 0; j < NN_HIDDEN1; j++) {
        float err = 0;
        for (int k = 0; k < NN_HIDDEN2; k++) {
            err += d2[k] * nn->w2[j][k];
        }
        d1[j] = err * relu_deriv(nn->a1[j]);
    }

    /* ── Update weights ───────────────────────────────────────────── */
    /* Layer 3: Hidden2 -> Output */
    for (int i = 0; i < NN_HIDDEN2; i++) {
        for (int j = 0; j < NN_OUTPUTS; j++) {
            nn->w3[i][j] += lr * d3[j] * nn->a2[i];
        }
    }
    for (int j = 0; j < NN_OUTPUTS; j++) {
        nn->b3[j] += lr * d3[j];
    }

    /* Layer 2: Hidden1 -> Hidden2 */
    for (int i = 0; i < NN_HIDDEN1; i++) {
        for (int j = 0; j < NN_HIDDEN2; j++) {
            nn->w2[i][j] += lr * d2[j] * nn->a1[i];
        }
    }
    for (int j = 0; j < NN_HIDDEN2; j++) {
        nn->b2[j] += lr * d2[j];
    }

    /* Layer 1: Input -> Hidden1 */
    for (int i = 0; i < NN_INPUTS; i++) {
        for (int j = 0; j < NN_HIDDEN1; j++) {
            nn->w1[i][j] += lr * d1[j] * nn->a0[i];
        }
    }
    for (int j = 0; j < NN_HIDDEN1; j++) {
        nn->b1[j] += lr * d1[j];
    }

    return totalError / NN_OUTPUTS;
}

float training_run(NeuralNet *nn, ReplayBuffer *rb, const PlayerProfile *prof) {
    if (rb->count == 0) return 0.0f;

    /* Get behavioral biases from player profile */
    float biases[NN_OUTPUTS];
    profile_get_training_bias(prof, biases);

    float totalLoss = 0;

    for (int epoch = 0; epoch < TRAINING_EPOCHS; epoch++) {
        float epochLoss = 0;

        for (int s = 0; s < rb->count; s++) {
            ReplaySample *sample = &rb->samples[s];

            /* Construct target outputs based on reward signal.
             * High reward = reinforce current outputs.
             * Low/negative reward = push away from current outputs.
             * Biases from player profile shift targets to counter habits. */
            float targets[NN_OUTPUTS];
            for (int j = 0; j < NN_OUTPUTS; j++) {
                float reinforcement = sample->outputs[j] + sample->reward * 0.1f;
                /* Apply behavioral counter-bias */
                reinforcement += biases[j] * 0.05f;
                /* Clamp to valid sigmoid range */
                if (reinforcement < 0.01f) reinforcement = 0.01f;
                if (reinforcement > 0.99f) reinforcement = 0.99f;
                targets[j] = reinforcement;
            }

            epochLoss += train_sample(nn, sample->inputs, targets, LEARNING_RATE);
        }

        totalLoss = epochLoss / rb->count;
    }

    return totalLoss;
}

void training_mutate(NeuralNet *nn, const PlayerProfile *prof, unsigned int seed) {
    srand(seed);

    /* Mutation intensity increases when the player keeps escaping */
    float intensity = MUTATION_RATE;
    if (prof->totalEscapes > 3) intensity *= 1.5f;
    if (prof->totalEscapes > 7) intensity *= 2.0f;

    /* Mutate Layer 1 weights */
    for (int i = 0; i < NN_INPUTS; i++) {
        for (int j = 0; j < NN_HIDDEN1; j++) {
            if ((rand() % 1000) < (int)(intensity * 1000)) {
                nn->w1[i][j] += ((float)rand() / RAND_MAX - 0.5f) * intensity * 2.0f;
            }
        }
    }

    /* Mutate Layer 2 weights */
    for (int i = 0; i < NN_HIDDEN1; i++) {
        for (int j = 0; j < NN_HIDDEN2; j++) {
            if ((rand() % 1000) < (int)(intensity * 1000)) {
                nn->w2[i][j] += ((float)rand() / RAND_MAX - 0.5f) * intensity * 2.0f;
            }
        }
    }

    /* Mutate Layer 3 weights */
    for (int i = 0; i < NN_HIDDEN2; i++) {
        for (int j = 0; j < NN_OUTPUTS; j++) {
            if ((rand() % 1000) < (int)(intensity * 1000)) {
                nn->w3[i][j] += ((float)rand() / RAND_MAX - 0.5f) * intensity * 2.0f;
            }
        }
    }
}
