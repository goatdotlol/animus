/*
 * EREBUS — Panic Room (Between-Run Hub)
 *
 * The Panic Room is a screen shown between runs where the player
 * can visualize the monster's neural network and perform
 * "synaptic surgery" — manually damaging/altering specific weights.
 *
 * This is the meta-game: you're not just surviving, you're
 * trying to lobotomize your pursuer.
 */
#ifndef GAME_PANICROOM_H
#define GAME_PANICROOM_H

#include "ai/neural_net.h"
#include "game/deathcard.h"

typedef struct {
    int   selectedLayer;     /* 0=w1, 1=w2, 2=w3 */
    int   selectedNeuronX;
    int   selectedNeuronY;
    int   surgeryPoints;     /* Earned by surviving longer */
    float viewScale;
    float viewOffsetX;
    float viewOffsetY;
    float glitchTimer;
} PanicRoom;

void panicroom_init(PanicRoom *pr, int surgeryPoints);

/* Draw the panic room. Returns true when player is done. */
bool panicroom_update_and_draw(PanicRoom *pr, NeuralNet *nn,
                                const PlayerProfile *prof, float dt);

#endif /* GAME_PANICROOM_H */
