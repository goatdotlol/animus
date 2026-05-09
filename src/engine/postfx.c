/*
 * EREBUS — Post-Processing Effects (Sprint 1 CPU implementation)
 * Full GPU shader pipeline comes in Sprint 8.
 * For now: scanlines, vignette, and noise applied directly to the pixel buffer.
 */
#include "engine/postfx.h"
#include <math.h>
#include <stdlib.h>

void postfx_init(PostFX *fx) {
    fx->enabled = true;
    fx->scanlineIntensity   = 0.15f;
    fx->barrelAmount        = 0.0f;   /* Off for now */
    fx->chromaticAberration = 0.0f;
    fx->vignetteStrength    = 0.4f;
    fx->noiseAmount         = 0.02f;
}

void postfx_apply(PostFX *fx, Color *buffer, int w, int h) {
    if (!fx->enabled) return;

    float halfW = w * 0.5f;
    float halfH = h * 0.5f;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            Color *px = &buffer[idx];

            float r = px->r / 255.0f;
            float g = px->g / 255.0f;
            float b = px->b / 255.0f;

            /* Scanlines: darken every other row */
            if (fx->scanlineIntensity > 0 && (y & 1)) {
                float sl = 1.0f - fx->scanlineIntensity;
                r *= sl; g *= sl; b *= sl;
            }

            /* Vignette: darken edges */
            if (fx->vignetteStrength > 0) {
                float dx = (x - halfW) / halfW;
                float dy = (y - halfH) / halfH;
                float dist = sqrtf(dx * dx + dy * dy);
                float vig = 1.0f - dist * fx->vignetteStrength * 0.7f;
                if (vig < 0) vig = 0;
                r *= vig; g *= vig; b *= vig;
            }

            /* Screen noise */
            if (fx->noiseAmount > 0) {
                float noise = ((rand() % 1000) / 1000.0f - 0.5f) * fx->noiseAmount;
                r += noise; g += noise; b += noise;
            }

            /* Clamp and write back */
            if (r < 0) r = 0; if (r > 1) r = 1;
            if (g < 0) g = 0; if (g > 1) g = 1;
            if (b < 0) b = 0; if (b > 1) b = 1;

            px->r = (unsigned char)(r * 255);
            px->g = (unsigned char)(g * 255);
            px->b = (unsigned char)(b * 255);
        }
    }
}
