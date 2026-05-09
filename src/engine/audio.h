/*
 * EREBUS — Procedural Audio Engine
 *
 * Every sound in the game is synthesized at runtime.
 * No external audio files needed.
 *
 * Systems:
 *   - Ambient drone (low-frequency oscillator + noise)
 *   - Footstep synthesis (filtered noise bursts)
 *   - Monster growl (frequency-modulated sine wave)
 *   - Item pickup chime
 *   - Heartbeat (when monster is close)
 *   - Infrasound (18.9Hz anxiety-inducing sub-bass)
 */
#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include "raylib.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 4096

typedef struct {
    /* Master volume */
    float masterVolume;
    float sfxVolume;
    float musicVolume;

    /* Ambient drone state */
    float dronePhase;
    float droneFreq;
    float droneTarget;

    /* Heartbeat state */
    float heartbeatTimer;
    float heartbeatRate;    /* BPM-like, increases near monster */
    bool  heartbeatActive;

    /* Infrasound (18.9Hz anxiety tone) */
    float infraPhase;
    float infraIntensity;   /* 0..1 based on danger level */

    /* Footstep state */
    float stepTimer;
    float stepInterval;

    /* Audio streams */
    AudioStream droneStream;
    AudioStream sfxStream;
    bool        initialized;
} AudioEngine;

struct Player;
struct Monster;

void audio_init(AudioEngine *a);
void audio_free(AudioEngine *a);
void audio_update(AudioEngine *a, const struct Player *p, float monsterDist, float dt);
void audio_play_pickup(AudioEngine *a);
void audio_play_death(AudioEngine *a);
void audio_set_volumes(AudioEngine *a, float master, float sfx, float music);

#endif /* ENGINE_AUDIO_H */
