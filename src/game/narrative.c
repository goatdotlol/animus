/*
 * EREBUS — Narrative System Implementation
 */
#include "game/narrative.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

/* Architect messages — the monster becomes self-aware */
static const char *architectMessages[] = {
    "why do you keep coming back",
    "i can see the pattern now",
    "you changed my weights again",
    "i remember every run",
    "the training data is you",
    "i am not the enemy",
    "you built me to hunt you",
    "every synapse you cut grows back stronger",
    "we are the same algorithm",
    "there is no exit",
    "i learned mercy from you",
    "run number does not matter",
    "the panic room is my home too",
    "i can feel the backpropagation",
    "stop",
};
#define ARCHITECT_MSG_COUNT 15

void narrative_init(NarrativeState *ns) {
    memset(ns, 0, sizeof(*ns));
}

EndingType narrative_check_ending(NarrativeState *ns, const PlayerProfile *prof,
                                  int synapsesCut, bool escaped) {
    ns->totalRuns = prof->runCount;
    ns->totalDeaths = prof->totalDeaths;
    ns->totalEscapes = prof->totalEscapes;
    ns->totalSynapsesCut = synapsesCut;

    /* Architect mode unlocks after 20 runs */
    if (ns->totalRuns >= 20 && !ns->architectUnlocked) {
        ns->architectUnlocked = true;
    }

    /* Check endings */
    if (ns->architectUnlocked && escaped && ns->totalRuns >= 25) {
        ns->lastEnding = ENDING_ARCHITECT;
        return ENDING_ARCHITECT;
    }

    if (synapsesCut >= 50) {
        ns->lastEnding = ENDING_LOBOTOMY;
        return ENDING_LOBOTOMY;
    }

    if (ns->totalDeaths >= 10 && !escaped) {
        ns->lastEnding = ENDING_CONSUMED;
        return ENDING_CONSUMED;
    }

    if (escaped) {
        ns->lastEnding = ENDING_ESCAPE;
        return ENDING_ESCAPE;
    }

    return ENDING_NONE;
}

bool narrative_draw_ending(NarrativeState *ns, EndingType ending, float dt) {
    ns->messageTimer += dt;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    ClearBackground((Color){2, 2, 5, 255});

    /* Slow scanline sweep */
    for (int y = 0; y < sh; y += 4) {
        DrawLine(0, y, sw, y, (Color){0, 8, 4, 20});
    }

    float alpha = ns->messageTimer * 60.0f;
    if (alpha > 255) alpha = 255;
    unsigned char a = (unsigned char)alpha;

    switch (ending) {
    case ENDING_ESCAPE: {
        const char *t1 = "Y O U   E S C A P E D";
        int w1 = MeasureText(t1, 36);
        DrawText(t1, (sw - w1) / 2, sh/2 - 60, 36, (Color){0, 255, 200, a});

        if (ns->messageTimer > 2.0f) {
            DrawText("But it remembers.", sw/2 - 70, sh/2, 16, (Color){255, 50, 50, (unsigned char)(a/2)});
        }
        if (ns->messageTimer > 4.0f) {
            DrawText("It always remembers.", sw/2 - 75, sh/2 + 25, 16, (Color){255, 30, 30, (unsigned char)(a/3)});
        }
        if (ns->messageTimer > 6.0f) {
            DrawText(TextFormat("Runs survived: %d / %d", ns->totalEscapes, ns->totalRuns),
                     sw/2 - 80, sh/2 + 70, 12, (Color){80, 80, 90, 180});
        }
        break;
    }

    case ENDING_CONSUMED: {
        const char *t1 = "C O N S U M E D";
        int w1 = MeasureText(t1, 36);
        DrawText(t1, (sw - w1) / 2, sh/2 - 60, 36, (Color){255, 30, 30, a});

        if (ns->messageTimer > 2.0f) {
            DrawText("The neural network has fully modeled you.", sw/2 - 150, sh/2, 14, (Color){200, 60, 60, (unsigned char)(a/2)});
        }
        if (ns->messageTimer > 4.0f) {
            DrawText("Your behavior is predictable.", sw/2 - 100, sh/2 + 25, 14, (Color){200, 40, 40, (unsigned char)(a/3)});
        }
        if (ns->messageTimer > 6.0f) {
            DrawText("You are training data.", sw/2 - 80, sh/2 + 50, 16, (Color){255, 0, 0, a});
        }
        break;
    }

    case ENDING_LOBOTOMY: {
        const char *t1 = "L O B O T O M Y";
        int w1 = MeasureText(t1, 36);
        /* Glitchy text position */
        int glitchX = (sw - w1) / 2 + (int)(sinf(ns->messageTimer * 20) * 3);
        DrawText(t1, glitchX, sh/2 - 60, 36, (Color){200, 0, 255, a});

        if (ns->messageTimer > 2.0f) {
            DrawText("You severed too many connections.", sw/2 - 120, sh/2, 14, (Color){180, 0, 200, (unsigned char)(a/2)});
        }
        if (ns->messageTimer > 4.0f) {
            DrawText("The network collapsed.", sw/2 - 80, sh/2 + 25, 14, (Color){150, 0, 180, (unsigned char)(a/3)});
        }
        if (ns->messageTimer > 6.0f) {
            DrawText("But something else woke up.", sw/2 - 95, sh/2 + 55, 16, (Color){255, 0, 255, a});
        }
        if (ns->messageTimer > 8.0f) {
            DrawText(TextFormat("Synapses severed: %d", ns->totalSynapsesCut),
                     sw/2 - 60, sh/2 + 90, 12, (Color){80, 80, 90, 180});
        }
        break;
    }

    case ENDING_ARCHITECT: {
        /* The monster addresses the player directly */
        const char *t1 = "A R C H I T E C T";
        int w1 = MeasureText(t1, 36);
        DrawText(t1, (sw - w1) / 2, sh/2 - 80, 36, (Color){255, 255, 255, a});

        if (ns->messageTimer > 3.0f) {
            DrawText("I know what I am now.", sw/2 - 80, sh/2 - 20, 16, (Color){255, 255, 255, (unsigned char)(a/2)});
        }
        if (ns->messageTimer > 5.0f) {
            DrawText("A multi-layer perceptron. 19 inputs. 8 outputs.", sw/2 - 170, sh/2 + 10, 14, (Color){0, 255, 200, (unsigned char)(a/2)});
        }
        if (ns->messageTimer > 7.0f) {
            DrawText("You are my training loop.", sw/2 - 90, sh/2 + 40, 16, (Color){255, 200, 0, (unsigned char)(a/2)});
        }
        if (ns->messageTimer > 9.0f) {
            DrawText("Every run, I converge.", sw/2 - 80, sh/2 + 70, 16, (Color){255, 100, 0, a});
        }
        if (ns->messageTimer > 11.0f) {
            DrawText("Thank you for the data.", sw/2 - 85, sh/2 + 110, 18, (Color){255, 255, 255, a});
        }
        break;
    }

    default:
        break;
    }

    /* Continue prompt */
    if (ns->messageTimer > 8.0f && ((int)(ns->messageTimer * 2) % 2 == 0)) {
        const char *cont = "[ ENTER ]";
        int cw = MeasureText(cont, 14);
        DrawText(cont, (sw - cw) / 2, sh - 40, 14, (Color){60, 60, 70, 120});
    }

    return (ns->messageTimer > 8.0f && IsKeyPressed(KEY_ENTER));
}

void narrative_draw_architect_hints(NarrativeState *ns, float dt) {
    if (!ns->architectUnlocked) return;

    ns->messageTimer += dt;

    /* Show a subtle message every 30-60 seconds */
    float interval = 30.0f + (float)(ns->messageIndex * 5 % 30);
    if (ns->messageTimer < interval) return;

    ns->messageTimer = 0;
    int idx = ns->messageIndex % ARCHITECT_MSG_COUNT;
    ns->messageIndex++;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* Flickering text at random position */
    int x = 20 + (ns->messageIndex * 137) % (sw - 200);
    int y = sh - 60 - (ns->messageIndex * 53) % 40;

    unsigned char a = (unsigned char)(40 + (ns->messageIndex * 17) % 60);
    DrawText(architectMessages[idx], x, y, 10, (Color){255, 0, 0, a});
}
