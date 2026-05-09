/*
 * EREBUS — Post-Processing Effects (v2: Dynamic Horror Effects)
 * CRT scanlines, barrel distortion, chromatic aberration, vignette.
 * Full implementation comes in Sprint 8; this provides the interface.
 */
#ifndef ENGINE_POSTFX_H
#define ENGINE_POSTFX_H

#include "raylib.h"

typedef struct {
    bool  enabled;
    float scanlineIntensity;    /* 0..1 — CRT scanline darkness      */
    float barrelAmount;         /* 0..1 — barrel/fisheye distortion   */
    float chromaticAberration;  /* 0..1 — RGB split amount            */
    float vignetteStrength;     /* 0..1 — edge darkening              */
    float noiseAmount;          /* 0..1 — screen noise/static         */

    /* Dynamic horror effects (driven by game state) */
    float dangerTint;           /* 0..1 — red overlay when monster is near */
    float glitchAmount;         /* 0..1 — visual corruption (overfitting) */
    float pulseTimer;           /* Internal timer for pulsing effects */
} PostFX;

void postfx_init(PostFX *fx);
void postfx_apply(PostFX *fx, Color *buffer, int w, int h);

#endif /* ENGINE_POSTFX_H */
