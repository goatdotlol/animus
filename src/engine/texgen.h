/*
 * EREBUS — Procedural Texture Generator
 * Generates wall, floor, and ceiling textures at startup using Perlin noise.
 * No external art files needed — the entire visual palette is mathematical.
 */
#ifndef ENGINE_TEXGEN_H
#define ENGINE_TEXGEN_H

#include "raylib.h"
#include "engine/raycaster.h"

/* ── Texture Types ────────────────────────────────────────────────── */
typedef enum {
    TEX_WALL_METAL,         /* Dark rusty metal panels                       */
    TEX_WALL_CONCRETE,      /* Rough gray concrete                           */
    TEX_WALL_LAB,           /* Clean lab panels with subtle grid              */
    TEX_WALL_BIO,           /* Organic, unsettling growth                     */
    TEX_WALL_DARK,          /* Very dark, almost featureless                  */
    TEX_WALL_DOOR,          /* Door texture (distinctive)                     */
    TEX_FLOOR_TILE,         /* Generic floor tiles                            */
    TEX_FLOOR_GRATE,        /* Metal grating                                  */
    TEX_CEIL_PANEL,         /* Ceiling panels                                 */
    TEX_CEIL_VENT,          /* Ventilation ceiling                             */
    TEX_COUNT               /* Total texture count                             */
} TextureType;

/* ── Public API ───────────────────────────────────────────────────── */

/* Generate all procedural textures and register them with the renderer.
 * Call once at startup. Uses a seed for reproducibility. */
void texgen_generate_all(RaycastRenderer *r, unsigned int seed);

/* Get a raw 64x64 pixel buffer for a specific texture type.
 * Only valid after texgen_generate_all has been called. */
Color *texgen_get_pixels(TextureType type);

/* Perlin noise utilities (exposed for other procedural uses) */
void  noise_seed(unsigned int seed);
float noise_2d(float x, float y);
float noise_fbm(float x, float y, int octaves, float lacunarity, float gain);

#endif /* ENGINE_TEXGEN_H */
