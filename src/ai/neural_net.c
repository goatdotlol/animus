/*
 * EREBUS — Neural Network Implementation
 */
#include "ai/neural_net.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

/* Activation functions */
static float relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/* Random float between -1 and 1 */
static float rand_weight() {
    return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
}

void nn_init(NeuralNet *nn, unsigned int seed) {
    srand(seed);

    /* Xavier initialization for better convergence */
    float limit1 = sqrtf(6.0f / (NN_INPUTS + NN_HIDDEN1));
    for (int i = 0; i < NN_INPUTS; i++) {
        for (int j = 0; j < NN_HIDDEN1; j++) {
            nn->w1[i][j] = rand_weight() * limit1;
        }
    }
    for (int j = 0; j < NN_HIDDEN1; j++) nn->b1[j] = 0.0f;

    float limit2 = sqrtf(6.0f / (NN_HIDDEN1 + NN_HIDDEN2));
    for (int i = 0; i < NN_HIDDEN1; i++) {
        for (int j = 0; j < NN_HIDDEN2; j++) {
            nn->w2[i][j] = rand_weight() * limit2;
        }
    }
    for (int j = 0; j < NN_HIDDEN2; j++) nn->b2[j] = 0.0f;

    float limit3 = sqrtf(6.0f / (NN_HIDDEN2 + NN_OUTPUTS));
    for (int i = 0; i < NN_HIDDEN2; i++) {
        for (int j = 0; j < NN_OUTPUTS; j++) {
            nn->w3[i][j] = rand_weight() * limit3;
        }
    }
    for (int j = 0; j < NN_OUTPUTS; j++) nn->b3[j] = 0.0f;
}

void nn_forward(NeuralNet *nn, const float inputs[NN_INPUTS], float outputs[NN_OUTPUTS]) {
    /* Copy inputs to a0 */
    for (int i = 0; i < NN_INPUTS; i++) {
        nn->a0[i] = inputs[i];
    }

    /* Layer 1: Input -> Hidden 1 (ReLU) */
    for (int j = 0; j < NN_HIDDEN1; j++) {
        float sum = nn->b1[j];
        for (int i = 0; i < NN_INPUTS; i++) {
            sum += nn->a0[i] * nn->w1[i][j];
        }
        nn->a1[j] = relu(sum);
    }

    /* Layer 2: Hidden 1 -> Hidden 2 (ReLU) */
    for (int j = 0; j < NN_HIDDEN2; j++) {
        float sum = nn->b2[j];
        for (int i = 0; i < NN_HIDDEN1; i++) {
            sum += nn->a1[i] * nn->w2[i][j];
        }
        nn->a2[j] = relu(sum);
    }

    /* Layer 3: Hidden 2 -> Output (Sigmoid for probabilities 0..1) */
    for (int j = 0; j < NN_OUTPUTS; j++) {
        float sum = nn->b3[j];
        for (int i = 0; i < NN_HIDDEN2; i++) {
            sum += nn->a2[i] * nn->w3[i][j];
        }
        nn->a3[j] = sigmoid(sum);
        outputs[j] = nn->a3[j];
    }
}

bool nn_save(const NeuralNet *nn, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    size_t written = fwrite(nn, sizeof(NeuralNet), 1, f);
    fclose(f);
    return written == 1;
}

bool nn_load(NeuralNet *nn, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    size_t read = fread(nn, sizeof(NeuralNet), 1, f);
    fclose(f);
    return read == 1;
}
