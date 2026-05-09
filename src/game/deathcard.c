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
