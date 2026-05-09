/*
 * EREBUS — Narrative System
 *
 * Manages story progression, endings, and the Architect Mode reveal.
 * The narrative emerges from how many runs the player has completed
 * and how the neural network has evolved.
 *
 * Endings:
 *   ENDING_ESCAPE   — Collect all cores and exit (standard win)
 *   ENDING_CONSUMED — Die 10+ times (the monster has fully learned you)
 *   ENDING_LOBOTOMY — Sever 50+ synapses in the Panic Room (you broke it)
 *   ENDING_ARCHITECT— Survive 20+ runs: the NN becomes self-aware
 */
#ifndef GAME_NARRATIVE_H
#define GAME_NARRATIVE_H

#include "game/deathcard.h"

typedef enum {
    ENDING_NONE,
    ENDING_ESCAPE,
    ENDING_CONSUMED,
    ENDING_LOBOTOMY,
    ENDING_ARCHITECT
} EndingType;

typedef struct {
    int  totalRuns;
    int  totalDeaths;
    int  totalEscapes;
    int  totalSynapsesCut;
    bool architectUnlocked;  /* NN has crossed self-awareness threshold */
    EndingType lastEnding;

    /* Architect mode: the monster starts leaving messages */
    int  messageIndex;
    float messageTimer;
} NarrativeState;

void narrative_init(NarrativeState *ns);

/* Check if any ending condition has been met. Updates state. */
EndingType narrative_check_ending(NarrativeState *ns, const PlayerProfile *prof,
                                  int synapsesCut, bool escaped);

/* Draw ending screen. Returns true when done. */
bool narrative_draw_ending(NarrativeState *ns, EndingType ending, float dt);

/* Draw architect messages during gameplay (subtle text flickers) */
void narrative_draw_architect_hints(NarrativeState *ns, float dt);

#endif /* GAME_NARRATIVE_H */
