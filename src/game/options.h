/*
 * EREBUS — Options Menu
 */
#ifndef GAME_OPTIONS_H
#define GAME_OPTIONS_H

#include "raylib.h"

typedef struct {
    float masterVolume;
    float sfxVolume;
    float musicVolume;
    bool  fullscreen;
    bool  showFPS;
    bool  showMinimap;
    float mouseSensitivity;
    bool  postFXEnabled;
    float scanlineIntensity;
    float vignetteStrength;
} GameOptions;

void options_init(GameOptions *opts);
bool options_save(const GameOptions *opts, const char *filename);
bool options_load(GameOptions *opts, const char *filename);
bool options_draw(GameOptions *opts);

#endif
