/*
 * EREBUS — Panic Room Implementation
 */
#include "game/panicroom.h"
#include "raylib.h"
#include <math.h>
#include <stdlib.h>

void panicroom_init(PanicRoom *pr, int surgeryPoints) {
    pr->selectedLayer = 0;
    pr->selectedNeuronX = 0;
    pr->selectedNeuronY = 0;
    pr->surgeryPoints = surgeryPoints;
    pr->viewScale = 1.0f;
    pr->viewOffsetX = 0;
    pr->viewOffsetY = 0;
    pr->glitchTimer = 0;
}

/* Map a weight value to a color */
static Color weight_to_color(float w) {
    if (w > 0) {
        int v = (int)(w * 400);
        if (v > 255) v = 255;
        return (Color){0, (unsigned char)v, (unsigned char)(v / 2), 255};
    } else {
        int v = (int)(-w * 400);
        if (v > 255) v = 255;
        return (Color){(unsigned char)v, 0, (unsigned char)(v / 3), 255};
    }
}

/* Draw a weight matrix as a heatmap */
static void draw_weight_grid(const float *weights, int rows, int cols,
                              int ox, int oy, int cellSize, int selX, int selY,
                              bool isSelected) {
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            float w = weights[y * cols + x];
            Color c = weight_to_color(w);
            DrawRectangle(ox + x * cellSize, oy + y * cellSize,
                          cellSize - 1, cellSize - 1, c);
        }
    }

    /* Selection highlight */
    if (isSelected && selX >= 0 && selX < cols && selY >= 0 && selY < rows) {
        DrawRectangleLines(ox + selX * cellSize - 1, oy + selY * cellSize - 1,
                           cellSize + 1, cellSize + 1, (Color){255, 255, 0, 200});
    }
}

bool panicroom_update_and_draw(PanicRoom *pr, NeuralNet *nn,
                                const PlayerProfile *prof, float dt) {
    pr->glitchTimer += dt;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* Input */
    if (IsKeyPressed(KEY_ONE))   pr->selectedLayer = 0;
    if (IsKeyPressed(KEY_TWO))   pr->selectedLayer = 1;
    if (IsKeyPressed(KEY_THREE)) pr->selectedLayer = 2;

    if (IsKeyPressed(KEY_RIGHT)) pr->selectedNeuronX++;
    if (IsKeyPressed(KEY_LEFT))  pr->selectedNeuronX--;
    if (IsKeyPressed(KEY_DOWN))  pr->selectedNeuronY++;
    if (IsKeyPressed(KEY_UP))    pr->selectedNeuronY--;

    /* Clamp selection */
    int maxX, maxY;
    switch (pr->selectedLayer) {
        case 0: maxX = NN_HIDDEN1; maxY = NN_INPUTS;  break;
        case 1: maxX = NN_HIDDEN2; maxY = NN_HIDDEN1; break;
        case 2: maxX = NN_OUTPUTS; maxY = NN_HIDDEN2; break;
        default: maxX = 1; maxY = 1; break;
    }
    if (pr->selectedNeuronX < 0) pr->selectedNeuronX = 0;
    if (pr->selectedNeuronX >= maxX) pr->selectedNeuronX = maxX - 1;
    if (pr->selectedNeuronY < 0) pr->selectedNeuronY = 0;
    if (pr->selectedNeuronY >= maxY) pr->selectedNeuronY = maxY - 1;

    /* Synaptic surgery: damage a weight */
    if (IsKeyPressed(KEY_SPACE) && pr->surgeryPoints > 0) {
        pr->surgeryPoints--;
        float *w = NULL;
        switch (pr->selectedLayer) {
            case 0: w = &nn->w1[pr->selectedNeuronY][pr->selectedNeuronX]; break;
            case 1: w = &nn->w2[pr->selectedNeuronY][pr->selectedNeuronX]; break;
            case 2: w = &nn->w3[pr->selectedNeuronY][pr->selectedNeuronX]; break;
        }
        if (w) {
            *w *= 0.1f; /* Severe damage */
            pr->glitchTimer = 0;
        }
    }

    /* Randomize a weight */
    if (IsKeyPressed(KEY_R) && pr->surgeryPoints > 0) {
        pr->surgeryPoints--;
        float *w = NULL;
        switch (pr->selectedLayer) {
            case 0: w = &nn->w1[pr->selectedNeuronY][pr->selectedNeuronX]; break;
            case 1: w = &nn->w2[pr->selectedNeuronY][pr->selectedNeuronX]; break;
            case 2: w = &nn->w3[pr->selectedNeuronY][pr->selectedNeuronX]; break;
        }
        if (w) {
            *w = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    }

    /* ── Draw ─────────────────────────────────────────────────────── */
    ClearBackground((Color){3, 3, 8, 255});

    /* Scanline effect */
    for (int y = 0; y < sh; y += 3) {
        DrawLine(0, y, sw, y, (Color){0, 10, 5, 30});
    }

    /* Title */
    const char *title = "P A N I C   R O O M";
    int tw = MeasureText(title, 24);
    Color titleCol = (Color){0, 255, 200, (unsigned char)(180 + (int)(sinf(pr->glitchTimer * 3) * 40))};
    DrawText(title, (sw - tw) / 2, 15, 24, titleCol);

    DrawText("// NEURAL TOPOLOGY VIEWER //", sw / 2 - 120, 45, 10, (Color){100, 50, 50, 150});

    /* Layer labels */
    const char *layerNames[] = {"[1] Input->Hidden1", "[2] Hidden1->Hidden2", "[3] Hidden2->Output"};
    for (int i = 0; i < 3; i++) {
        Color lc = (i == pr->selectedLayer) ? (Color){0, 255, 200, 255} : (Color){80, 80, 90, 180};
        DrawText(layerNames[i], 20 + i * 180, 65, 12, lc);
    }

    /* Draw weight matrices */
    int cellSize = 4;
    int gridY = 90;

    DrawText("Layer 1 (Input->H1)", 20, gridY - 15, 10, (Color){60, 60, 70, 180});
    draw_weight_grid((float*)nn->w1, NN_INPUTS, NN_HIDDEN1,
                     20, gridY, cellSize, pr->selectedNeuronX, pr->selectedNeuronY,
                     pr->selectedLayer == 0);

    int grid2Y = gridY + NN_INPUTS * cellSize + 20;
    DrawText("Layer 2 (H1->H2)", 20, grid2Y - 15, 10, (Color){60, 60, 70, 180});
    draw_weight_grid((float*)nn->w2, NN_HIDDEN1, NN_HIDDEN2,
                     20, grid2Y, cellSize, pr->selectedNeuronX, pr->selectedNeuronY,
                     pr->selectedLayer == 1);

    int grid3Y = grid2Y + NN_HIDDEN1 * cellSize + 20;
    DrawText("Layer 3 (H2->Out)", 20, grid3Y - 15, 10, (Color){60, 60, 70, 180});
    draw_weight_grid((float*)nn->w3, NN_HIDDEN2, NN_OUTPUTS,
                     20, grid3Y, cellSize * 2, pr->selectedNeuronX, pr->selectedNeuronY,
                     pr->selectedLayer == 2);

    /* Selected weight info */
    float selectedVal = 0;
    switch (pr->selectedLayer) {
        case 0: selectedVal = nn->w1[pr->selectedNeuronY][pr->selectedNeuronX]; break;
        case 1: selectedVal = nn->w2[pr->selectedNeuronY][pr->selectedNeuronX]; break;
        case 2: selectedVal = nn->w3[pr->selectedNeuronY][pr->selectedNeuronX]; break;
    }

    int infoX = sw - 280;
    DrawText("SELECTED SYNAPSE", infoX, 90, 14, (Color){0, 255, 200, 200});
    DrawText(TextFormat("Layer: %d", pr->selectedLayer + 1), infoX, 115, 12, (Color){150, 150, 160, 200});
    DrawText(TextFormat("Neuron: [%d, %d]", pr->selectedNeuronY, pr->selectedNeuronX), infoX, 130, 12, (Color){150, 150, 160, 200});
    DrawText(TextFormat("Weight: %.4f", selectedVal), infoX, 145, 12,
             selectedVal > 0 ? (Color){0, 200, 100, 255} : (Color){255, 80, 80, 255});

    /* Surgery controls */
    DrawText("SYNAPTIC SURGERY", infoX, 190, 14, (Color){255, 100, 100, 200});
    DrawText(TextFormat("Points: %d", pr->surgeryPoints), infoX, 215, 12, (Color){255, 200, 80, 200});
    DrawText("[SPACE] Sever connection", infoX, 240, 11, (Color){120, 120, 130, 180});
    DrawText("[R] Randomize weight", infoX, 255, 11, (Color){120, 120, 130, 180});
    DrawText("[Arrows] Navigate", infoX, 270, 11, (Color){120, 120, 130, 180});
    DrawText("[1/2/3] Select layer", infoX, 285, 11, (Color){120, 120, 130, 180});

    /* Run history */
    DrawText("RUN HISTORY", infoX, 330, 14, (Color){0, 255, 200, 200});
    int n = prof->runCount;
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
        int idx = prof->runCount - 1 - i;
        if (idx < 0 || idx >= MAX_RUN_HISTORY) continue;
        const RunRecord *r = &prof->runs[idx];
        const char *reason = r->reason == DEATH_SURVIVED ? "ESCAPED" :
                             r->reason == DEATH_CAUGHT ? "CAUGHT" : "TIMEOUT";
        Color rc = r->reason == DEATH_SURVIVED ? (Color){0, 255, 200, 180} : (Color){255, 80, 80, 180};
        DrawText(TextFormat("#%d %s %.0fs", r->runNumber, reason, r->survivalTime),
                 infoX, 355 + i * 16, 11, rc);
    }

    /* Continue hint */
    if ((int)(pr->glitchTimer * 2) % 2 == 0) {
        const char *cont = "[ ENTER ] Begin Next Run";
        int cw = MeasureText(cont, 14);
        DrawText(cont, (sw - cw) / 2, sh - 35, 14, (Color){0, 200, 160, 180});
    }

    return IsKeyPressed(KEY_ENTER);
}
