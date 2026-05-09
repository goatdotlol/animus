/*
 * EREBUS — Post-Processing Effects (v2: Dynamic Horror)
 *
 * New effects:
 *   - Chromatic aberration: RGB channel splitting, increases near monster
 *   - Danger tint: Red overlay that pulses when the monster is close
 *   - Glitch corruption: Scanline displacement + color inversion during
 *     monster overfitting/glitch state
 *   - Barrel distortion: Subtle fisheye at screen edges
 */
#include "engine/postfx.h"
#include <math.h>
#include <stdlib.h>

void postfx_init(PostFX *fx) {
    fx->enabled = true;
    fx->scanlineIntensity   = 0.15f;
    fx->barrelAmount        = 0.02f;
    fx->chromaticAberration = 0.0f;
    fx->vignetteStrength    = 0.4f;
    fx->noiseAmount         = 0.02f;
    fx->dangerTint          = 0.0f;
    fx->glitchAmount        = 0.0f;
    fx->pulseTimer          = 0.0f;
}

void postfx_apply(PostFX *fx, Color *buffer, int w, int h) {
    if (!fx->enabled) return;

    fx->pulseTimer += 0.016f; /* ~60fps tick */

    float halfW = w * 0.5f;
    float halfH = h * 0.5f;

    /* Glitch: displace random scanlines */
    if (fx->glitchAmount > 0.1f) {
        for (int i = 0; i < (int)(fx->glitchAmount * 15); i++) {
            int glitchY = rand() % h;
            int offset = (rand() % 20) - 10;
            if (offset == 0) continue;

            /* Shift the scanline horizontally */
            Color temp[640]; /* Max width we support */
            int clampedW = (w > 640) ? 640 : w;
            for (int x = 0; x < clampedW; x++) {
                int srcX = x - offset;
                if (srcX < 0) srcX = 0;
                if (srcX >= w) srcX = w - 1;
                temp[x] = buffer[glitchY * w + srcX];
            }
            for (int x = 0; x < clampedW; x++) {
                buffer[glitchY * w + x] = temp[x];
            }
        }

        /* Random color block corruption */
        if (fx->glitchAmount > 0.5f) {
            int blockCount = (int)(fx->glitchAmount * 5);
            for (int i = 0; i < blockCount; i++) {
                int bx = rand() % w;
                int by = rand() % h;
                int bw = 10 + rand() % 30;
                int bh = 2 + rand() % 5;
                Color glitchCol = (Color){
                    (unsigned char)(rand() % 255),
                    0,
                    (unsigned char)(rand() % 100),
                    255
                };
                for (int yy = by; yy < by + bh && yy < h; yy++) {
                    for (int xx = bx; xx < bx + bw && xx < w; xx++) {
                        buffer[yy * w + xx] = glitchCol;
                    }
                }
            }
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;

            float dx = (x - halfW) / halfW;
            float dy = (y - halfH) / halfH;
            float edgeDist = sqrtf(dx * dx + dy * dy);

            /* Chromatic aberration: read R/G/B from slightly offset positions */
            float r, g, b;
            if (fx->chromaticAberration > 0.01f) {
                int caOffset = (int)(fx->chromaticAberration * 3.0f + 1.0f);
                int rIdx = y * w + (x - caOffset < 0 ? 0 : x - caOffset);
                int bIdx = y * w + (x + caOffset >= w ? w - 1 : x + caOffset);
                r = buffer[rIdx].r / 255.0f;
                g = buffer[idx].g / 255.0f;
                b = buffer[bIdx].b / 255.0f;
            } else {
                Color *px = &buffer[idx];
                r = px->r / 255.0f;
                g = px->g / 255.0f;
                b = px->b / 255.0f;
            }

            /* Scanlines: darken every other row with intensity variation */
            if (fx->scanlineIntensity > 0 && (y & 1)) {
                float sl = 1.0f - fx->scanlineIntensity;
                r *= sl; g *= sl; b *= sl;
            }

            /* Barrel distortion (shift colors at edges) */
            if (fx->barrelAmount > 0.001f) {
                float barrel = 1.0f + edgeDist * edgeDist * fx->barrelAmount;
                /* Subtle darkening at distorted edges */
                float edgeFade = 1.0f / barrel;
                r *= edgeFade; g *= edgeFade; b *= edgeFade;
            }

            /* Vignette: darken edges */
            if (fx->vignetteStrength > 0) {
                float vig = 1.0f - edgeDist * fx->vignetteStrength * 0.7f;
                if (vig < 0) vig = 0;
                r *= vig; g *= vig; b *= vig;
            }

            /* Danger tint: pulsing red overlay when monster is close */
            if (fx->dangerTint > 0.01f) {
                float pulse = 0.5f + 0.5f * sinf(fx->pulseTimer * 4.0f);
                float tint = fx->dangerTint * pulse * 0.3f;
                r += tint;
                g *= (1.0f - fx->dangerTint * 0.2f);
                b *= (1.0f - fx->dangerTint * 0.3f);
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

            buffer[idx].r = (unsigned char)(r * 255);
            buffer[idx].g = (unsigned char)(g * 255);
            buffer[idx].b = (unsigned char)(b * 255);
        }
    }
}
