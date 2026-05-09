/*
 * EREBUS — Procedural Audio Engine Implementation
 */
#include "engine/audio.h"
#include "game/player.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979f

/* ── Synthesis helpers ────────────────────────────────────────────── */
static float sine_wave(float phase, float freq) {
    return sinf(2.0f * PI * freq * phase);
}

static float noise(void) {
    return ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
}

/* Simple one-pole low-pass filter */
static float lpf_state = 0;
static float low_pass(float input, float cutoff) {
    float rc = 1.0f / (2.0f * PI * cutoff);
    float dt = 1.0f / AUDIO_SAMPLE_RATE;
    float alpha = dt / (rc + dt);
    lpf_state = lpf_state + alpha * (input - lpf_state);
    return lpf_state;
}

void audio_init(AudioEngine *a) {
    memset(a, 0, sizeof(*a));
    a->masterVolume = 0.7f;
    a->sfxVolume = 0.8f;
    a->musicVolume = 0.5f;

    a->droneFreq = 55.0f;   /* A1 - deep bass drone */
    a->droneTarget = 55.0f;
    a->dronePhase = 0;

    a->heartbeatRate = 60.0f;
    a->heartbeatActive = false;

    a->infraIntensity = 0.0f;
    a->infraPhase = 0;

    a->stepInterval = 0.5f;
    a->stepTimer = 0;

    InitAudioDevice();

    /* Create audio streams for real-time synthesis */
    a->droneStream = LoadAudioStream(AUDIO_SAMPLE_RATE, 16, 1);
    a->sfxStream   = LoadAudioStream(AUDIO_SAMPLE_RATE, 16, 1);

    PlayAudioStream(a->droneStream);
    PlayAudioStream(a->sfxStream);

    a->initialized = true;
}

void audio_free(AudioEngine *a) {
    if (!a->initialized) return;
    UnloadAudioStream(a->droneStream);
    UnloadAudioStream(a->sfxStream);
    CloseAudioDevice();
    a->initialized = false;
}

void audio_set_volumes(AudioEngine *a, float master, float sfx, float music) {
    a->masterVolume = master;
    a->sfxVolume = sfx;
    a->musicVolume = music;
}

void audio_update(AudioEngine *a, const Player *p, float monsterDist, float dt) {
    if (!a->initialized) return;

    /* ── Update danger-responsive parameters ──────────────────────── */

    /* Heartbeat activates when monster is within 8 cells */
    if (monsterDist < 8.0f) {
        a->heartbeatActive = true;
        /* BPM increases as monster gets closer */
        a->heartbeatRate = 60.0f + (8.0f - monsterDist) * 20.0f;
    } else {
        a->heartbeatActive = false;
    }

    /* Infrasound ramps up when monster is within 12 cells */
    if (monsterDist < 12.0f) {
        a->infraIntensity = (12.0f - monsterDist) / 12.0f;
    } else {
        a->infraIntensity *= 0.95f; /* Fade out */
    }

    /* Drone frequency shifts with danger */
    a->droneTarget = 55.0f - monsterDist * 0.5f;
    if (a->droneTarget < 30.0f) a->droneTarget = 30.0f;
    a->droneFreq += (a->droneTarget - a->droneFreq) * dt * 2.0f;

    /* Footstep timing */
    if (p->isMoving) {
        float stepMult = 1.0f;
        switch (p->moveMode) {
            case MOVE_SPRINT: stepMult = 0.3f; break;
            case MOVE_WALK:   stepMult = 0.5f; break;
            case MOVE_CROUCH: stepMult = 0.8f; break;
            case MOVE_FREEZE: stepMult = 999.0f; break;
        }
        a->stepInterval = stepMult;
    }

    /* ── Fill drone audio stream ──────────────────────────────────── */
    if (IsAudioStreamProcessed(a->droneStream)) {
        short buffer[AUDIO_BUFFER_SIZE];
        float vol = a->masterVolume * a->musicVolume;

        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            float t = a->dronePhase;
            a->dronePhase += 1.0f / AUDIO_SAMPLE_RATE;

            /* Base drone: detuned sine waves */
            float drone = sine_wave(t, a->droneFreq) * 0.3f;
            drone += sine_wave(t, a->droneFreq * 1.005f) * 0.2f; /* Slight detune */
            drone += sine_wave(t, a->droneFreq * 0.5f) * 0.15f;  /* Sub octave */

            /* Add filtered noise for texture */
            float n = noise() * 0.05f;
            drone += low_pass(n, 200.0f);

            /* Infrasound: 18.9Hz sub-bass (below hearing, felt as anxiety) */
            if (a->infraIntensity > 0.01f) {
                a->infraPhase += 1.0f / AUDIO_SAMPLE_RATE;
                drone += sine_wave(a->infraPhase, 18.9f) * a->infraIntensity * 0.4f;
            }

            /* Heartbeat: low thump */
            if (a->heartbeatActive) {
                a->heartbeatTimer += 1.0f / AUDIO_SAMPLE_RATE;
                float beatPeriod = 60.0f / a->heartbeatRate;
                float beatPhase = fmodf(a->heartbeatTimer, beatPeriod) / beatPeriod;

                /* Double-thump pattern (lub-dub) */
                if (beatPhase < 0.05f) {
                    float env = 1.0f - beatPhase / 0.05f;
                    drone += sine_wave(a->heartbeatTimer, 40.0f) * env * 0.5f;
                } else if (beatPhase > 0.1f && beatPhase < 0.15f) {
                    float env = 1.0f - (beatPhase - 0.1f) / 0.05f;
                    drone += sine_wave(a->heartbeatTimer, 35.0f) * env * 0.3f;
                }
            }

            /* Footstep synthesis */
            if (p->isMoving && p->moveMode != MOVE_FREEZE) {
                a->stepTimer += 1.0f / AUDIO_SAMPLE_RATE;
                if (a->stepTimer > a->stepInterval) {
                    a->stepTimer = 0;
                }
                /* Short noise burst at step trigger */
                if (a->stepTimer < 0.03f) {
                    float env = 1.0f - a->stepTimer / 0.03f;
                    float step = noise() * env * 0.15f;
                    step = low_pass(step, 800.0f);
                    drone += step;
                }
            }

            /* Clamp and convert to 16-bit */
            drone *= vol;
            if (drone > 0.95f) drone = 0.95f;
            if (drone < -0.95f) drone = -0.95f;
            buffer[i] = (short)(drone * 32000);
        }

        UpdateAudioStream(a->droneStream, buffer, AUDIO_BUFFER_SIZE);
    }
}

void audio_play_pickup(AudioEngine *a) {
    if (!a->initialized) return;

    /* Generate a short ascending chime */
    if (IsAudioStreamProcessed(a->sfxStream)) {
        short buffer[AUDIO_BUFFER_SIZE];
        float vol = a->masterVolume * a->sfxVolume;
        int len = AUDIO_SAMPLE_RATE / 4; /* 0.25 seconds */
        if (len > AUDIO_BUFFER_SIZE) len = AUDIO_BUFFER_SIZE;

        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            float t = (float)i / AUDIO_SAMPLE_RATE;
            float sample = 0;

            if (i < len) {
                float env = 1.0f - (float)i / len;
                env *= env; /* Exponential decay */
                float freq = 800.0f + t * 2000.0f; /* Rising pitch */
                sample = sine_wave(t, freq) * env * 0.3f;
                sample += sine_wave(t, freq * 1.5f) * env * 0.1f; /* Harmonic */
            }

            sample *= vol;
            buffer[i] = (short)(sample * 32000);
        }

        UpdateAudioStream(a->sfxStream, buffer, AUDIO_BUFFER_SIZE);
    }
}

void audio_play_death(AudioEngine *a) {
    if (!a->initialized) return;

    if (IsAudioStreamProcessed(a->sfxStream)) {
        short buffer[AUDIO_BUFFER_SIZE];
        float vol = a->masterVolume * a->sfxVolume;

        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            float t = (float)i / AUDIO_SAMPLE_RATE;
            float sample = 0;

            float env = 1.0f - t * 2.0f;
            if (env < 0) env = 0;

            /* Descending distorted tone */
            float freq = 200.0f - t * 300.0f;
            if (freq < 20.0f) freq = 20.0f;
            sample = sine_wave(t, freq) * env * 0.5f;
            sample += noise() * env * 0.3f; /* Distortion */

            sample *= vol;
            if (sample > 0.95f) sample = 0.95f;
            if (sample < -0.95f) sample = -0.95f;
            buffer[i] = (short)(sample * 32000);
        }

        UpdateAudioStream(a->sfxStream, buffer, AUDIO_BUFFER_SIZE);
    }
}
