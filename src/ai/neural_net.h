/*
 * EREBUS — Multi-Layer Perceptron (MLP)
 *
 * This is the actual brain of the monster.
 * Architecture:
 *   - Input Layer:  19 neurons (player behavior stats, distance, light, etc.)
 *   - Hidden Layer: 32 neurons
 *   - Hidden Layer: 16 neurons
 *   - Output Layer:  8 neurons (action probabilities / weights)
 *
 * It uses standard feedforward execution and can be trained via backpropagation
 * using a replay buffer between runs.
 */
#ifndef AI_NEURAL_NET_H
#define AI_NEURAL_NET_H

#include <stdbool.h>

#define NN_INPUTS   19
#define NN_HIDDEN1  32
#define NN_HIDDEN2  16
#define NN_OUTPUTS  8

typedef struct {
    /* Weights */
    float w1[NN_INPUTS][NN_HIDDEN1];
    float w2[NN_HIDDEN1][NN_HIDDEN2];
    float w3[NN_HIDDEN2][NN_OUTPUTS];

    /* Biases */
    float b1[NN_HIDDEN1];
    float b2[NN_HIDDEN2];
    float b3[NN_OUTPUTS];

    /* Activations (stored for backprop) */
    float a0[NN_INPUTS];
    float a1[NN_HIDDEN1];
    float a2[NN_HIDDEN2];
    float a3[NN_OUTPUTS];
} NeuralNet;

/* ── API ──────────────────────────────────────────────────────────── */

/* Initialize weights randomly (Glorot/Xavier initialization) */
void nn_init(NeuralNet *nn, unsigned int seed);

/* Forward pass: compute outputs from inputs */
void nn_forward(NeuralNet *nn, const float inputs[NN_INPUTS], float outputs[NN_OUTPUTS]);

/* Save/Load weights to file (for persistence between runs) */
bool nn_save(const NeuralNet *nn, const char *filename);
bool nn_load(NeuralNet *nn, const char *filename);

#endif /* AI_NEURAL_NET_H */
