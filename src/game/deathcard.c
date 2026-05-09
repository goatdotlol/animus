/*
 * EREBUS — Death Card / Run Testimony Implementation
 */
#include "game/deathcard.h"
#include <stdio.h>
#include <string.h>

void profile_init(PlayerProfile *prof) {
    memset(prof, 0, sizeof(*prof));
    prof->bestTime = 999999.0f;
    prof->worstTime = 0.0f;
}

void profile_record_run(PlayerProfile *prof, const RunRecord *record) {
    if (prof->runCount < MAX_RUN_HISTORY) {
        prof->runs[prof->runCount] = *record;
    }
    prof->runCount++;

    if (record->reason == DEATH_SURVIVED) {
        prof->totalEscapes++;
        if (record->survivalTime < prof->bestTime)
            prof->bestTime = record->survivalTime;
    } else {
        prof->totalDeaths++;
    }

    if (record->survivalTime > prof->worstTime)
        prof->worstTime = record->survivalTime;
}

bool profile_save(const PlayerProfile *prof, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    fwrite(prof, sizeof(PlayerProfile), 1, f);
    fclose(f);
    return true;
}

bool profile_load(PlayerProfile *prof, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    size_t r = fread(prof, sizeof(PlayerProfile), 1, f);
    fclose(f);
    return r == 1;
}

void profile_get_training_bias(const PlayerProfile *prof, float biases[NN_OUTPUTS]) {
    /* Analyze run history to generate biases that make the monster
     * counter the player's dominant strategies.
     *
     * If player sprints a lot: increase aggression (output 0)
     * If player hides a lot: increase investigation thoroughness (output 2)
     * If player panics: exploit panic with fake retreats (output 3)
     */
    for (int i = 0; i < NN_OUTPUTS; i++) biases[i] = 0.0f;

    int n = prof->runCount;
    if (n <= 0) return;
    if (n > MAX_RUN_HISTORY) n = MAX_RUN_HISTORY;

    /* Weight recent runs more heavily */
    float totalSprint = 0, totalHide = 0, totalPanic = 0;
    float weight = 1.0f;
    float weightSum = 0;

    for (int i = n - 1; i >= 0 && i >= n - 10; i--) {
        totalSprint += prof->runs[i].timeSprinting * weight;
        totalHide   += prof->runs[i].timeHiding * weight;
        totalPanic  += prof->runs[i].avgPanic * weight;
        weightSum += weight;
        weight *= 0.8f; /* Exponential decay */
    }

    if (weightSum > 0) {
        totalSprint /= weightSum;
        totalHide   /= weightSum;
        totalPanic  /= weightSum;
    }

    /* Sprint counter: boost aggression + chase speed */
    biases[0] = totalSprint * 0.02f;   /* Aggression boost */

    /* Hide counter: boost investigation + patrol thoroughness */
    biases[2] = totalHide * 0.03f;     /* Investigation bias */

    /* Panic counter: exploit erratic behavior */
    biases[3] = totalPanic * 0.5f;     /* Panic exploitation */
}

void profile_get_fingerprint(const PlayerProfile *prof, char *outBuffer) {
    /* Create a pseudo-hash from stats */
    unsigned int h1 = prof->totalDeaths * 31337 + prof->totalEscapes * 7919;
    unsigned int h2 = (unsigned int)(prof->bestTime * 100.0f) ^ (unsigned int)(prof->worstTime * 10.0f);
    
    float totalSprint = 0, totalHide = 0, totalPanic = 0;
    for (int i = 0; i < prof->runCount && i < MAX_RUN_HISTORY; i++) {
        totalSprint += prof->runs[i].timeSprinting;
        totalHide += prof->runs[i].timeHiding;
        totalPanic += prof->runs[i].avgPanic;
    }
    unsigned int h3 = (unsigned int)(totalSprint) ^ (unsigned int)(totalHide * 3.0f);
    unsigned int h4 = (unsigned int)(totalPanic * 1000.0f);
    
    sprintf(outBuffer, "%04X-%04X-%04X-%04X", h1 & 0xFFFF, h2 & 0xFFFF, h3 & 0xFFFF, h4 & 0xFFFF);
}

/* ── Testimony System ────────────────────────────────────────────────── */
#include "ai/training.h"
#include "raylib.h"

void testimony_init(TestimonyState *t) {
    memset(t, 0, sizeof(*t));
    t->phase = 0; /* Init delay */
    t->questionId = GetRandomValue(0, 2); /* Random question */
}

bool testimony_update_and_draw(TestimonyState *t, ReplayBuffer *rb, float dt) {
    t->timer += dt;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    
    if (t->phase == 0) {
        if (t->timer > 1.5f) {
            t->phase = 1;
            t->timer = 0;
        }
        return false;
    }
    
    if (t->phase == 1) {
        const char *questions[3] = {
            "Why did you use the flare there?",
            "Why did you run North?",
            "Why did you hide?"
        };
        const char *answers[3][3] = {
            {"I was cornered.", "I thought you were there.", "Panic."},
            {"It was the only way out.", "I heard something.", "I didn't think."},
            {"I was afraid.", "I needed to rest.", "To watch you."}
        };
        
        DrawText("ANIMUS TESTIMONY", sw/2 - 120, sh/2 - 150, 20, (Color){255, 0, 0, 150});
        DrawText(questions[t->questionId], sw/2 - MeasureText(questions[t->questionId], 24)/2, sh/2 - 80, 24, WHITE);
        
        for (int i = 0; i < 3; i++) {
            Color col = (i == t->selectedAnswer) ? (Color){0, 255, 200, 255} : (Color){100, 100, 100, 255};
            DrawText(answers[t->questionId][i], sw/2 - MeasureText(answers[t->questionId][i], 20)/2, sh/2 + i * 40, 20, col);
        }
        
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) t->selectedAnswer = (t->selectedAnswer + 2) % 3;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) t->selectedAnswer = (t->selectedAnswer + 1) % 3;
        
        if (IsKeyPressed(KEY_ENTER)) {
            /* Inject massive bias sample into ReplayBuffer based on answer */
            float obs[NN_INPUTS] = {0};
            float action[NN_OUTPUTS] = {0};
            
            if (t->questionId == 0) action[2] = 1.0f; /* Investigation */
            else if (t->questionId == 1) action[0] = 1.0f; /* Aggression */
            else action[3] = 1.0f; /* Panic exploit */
            
            if (t->selectedAnswer == 2) action[1] = 1.0f; /* Defense */
            
            replay_add(rb, obs, action, 10.0f); /* Massive reward */
            
            t->phase = 2;
            t->timer = 0;
        }
        return false;
    }
    
    if (t->phase == 2) {
        DrawText("RECORDED.", sw/2 - MeasureText("RECORDED.", 30)/2, sh/2, 30, (Color){255, 0, 0, 200});
        if (t->timer > 1.5f) {
            t->done = true;
            return true;
        }
    }
    
    return false;
}
