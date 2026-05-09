/*
 * EREBUS — Procedural Texture Generator
 *
 * Generates all wall/floor/ceiling textures at startup using Perlin noise.
 * Each texture type has a unique color palette + noise configuration
 * that creates a distinct material feel — rusty metal, cracked concrete,
 * clean lab panels, organic growth, etc.
 *
 * The entire visual palette of the game is mathematical.
 * No external image files required.
 */
#include "engine/texgen.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ── Perlin Noise Implementation ──────────────────────────────────── */
static int perm[512];

void noise_seed(unsigned int seed) {
    srand(seed);
    int p[256];
    for (int i = 0; i < 256; i++) p[i] = i;
    /* Fisher-Yates shuffle */
    for (int i = 255; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = p[i]; p[i] = p[j]; p[j] = tmp;
    }
    for (int i = 0; i < 512; i++) perm[i] = p[i & 255];
}

static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static float lerp(float a, float b, float t) { return a + t * (b - a); }

static float grad2(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

float noise_2d(float x, float y) {
    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    x -= floorf(x);
    y -= floorf(y);
    float u = fade(x), v = fade(y);
    int aa = perm[perm[X] + Y];
    int ab = perm[perm[X] + Y + 1];
    int ba = perm[perm[X + 1] + Y];
    int bb = perm[perm[X + 1] + Y + 1];
    return lerp(
        lerp(grad2(aa, x, y),     grad2(ba, x - 1, y),     u),
        lerp(grad2(ab, x, y - 1), grad2(bb, x - 1, y - 1), u),
        v
    ) * 0.5f + 0.5f; /* Normalize to 0..1 */
}

float noise_fbm(float x, float y, int octaves, float lacunarity, float gain) {
    float sum = 0, amp = 1.0f, freq = 1.0f, maxAmp = 0;
    for (int i = 0; i < octaves; i++) {
        sum += noise_2d(x * freq, y * freq) * amp;
        maxAmp += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return sum / maxAmp;
}

/* ── Storage for generated textures ───────────────────────────────── */
static Color texStore[TEX_COUNT][RC_TEX_SIZE * RC_TEX_SIZE];

Color *texgen_get_pixels(TextureType type) {
    return texStore[type];
}

/* ── Color helpers ────────────────────────────────────────────────── */
static Color rgb(int r, int g, int b) { return (Color){r, g, b, 255}; }

static Color color_lerp(Color a, Color b, float t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    return rgb(
        (int)(a.r + (b.r - a.r) * t),
        (int)(a.g + (b.g - a.g) * t),
        (int)(a.b + (b.b - a.b) * t)
    );
}

static Color color_add(Color a, int dr, int dg, int db) {
    int r = a.r + dr; if (r < 0) r = 0; if (r > 255) r = 255;
    int g = a.g + dg; if (g < 0) g = 0; if (g > 255) g = 255;
    int b = a.b + db; if (b < 0) b = 0; if (b > 255) b = 255;
    return rgb(r, g, b);
}

/* ── Individual texture generators ────────────────────────────────── */

static void gen_wall_metal(Color *out, unsigned int seed) {
    /* Dark gray-brown industrial metal with rust patches */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float fx = x / (float)RC_TEX_SIZE;
            float fy = y / (float)RC_TEX_SIZE;

            /* Base metal noise */
            float n = noise_fbm(fx * 4 + seed, fy * 4, 4, 2.0f, 0.5f);

            /* Rust patches */
            float rust = noise_fbm(fx * 8 + seed * 2, fy * 8 + 50, 3, 2.5f, 0.4f);
            rust = (rust > 0.6f) ? (rust - 0.6f) * 2.5f : 0;

            /* Panel lines (horizontal and vertical every 16px) */
            float lineX = (x % 16 == 0) ? 0.7f : 1.0f;
            float lineY = (y % 16 == 0) ? 0.7f : 1.0f;

            Color base = color_lerp(rgb(35, 35, 40), rgb(55, 52, 48), n);
            base = color_lerp(base, rgb(80, 40, 20), rust);

            int br = (int)(base.r * lineX * lineY);
            int bg = (int)(base.g * lineX * lineY);
            int bb = (int)(base.b * lineX * lineY);

            out[y * RC_TEX_SIZE + x] = rgb(br, bg, bb);
        }
    }
}

static void gen_wall_concrete(Color *out, unsigned int seed) {
    /* Rough gray concrete with cracks */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float fx = x / (float)RC_TEX_SIZE;
            float fy = y / (float)RC_TEX_SIZE;

            float n = noise_fbm(fx * 6 + seed, fy * 6, 5, 2.0f, 0.55f);
            float crack = noise_fbm(fx * 16 + seed * 3, fy * 16, 2, 3.0f, 0.3f);
            crack = (crack < 0.15f) ? 0.3f : 1.0f;

            int gray = 30 + (int)(n * 40);
            out[y * RC_TEX_SIZE + x] = rgb(
                (int)(gray * crack),
                (int)(gray * crack * 0.98f),
                (int)((gray + 2) * crack)
            );
        }
    }
}

static void gen_wall_lab(Color *out, unsigned int seed) {
    /* Clean white-blue lab panels with subtle grid */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float fx = x / (float)RC_TEX_SIZE;
            float fy = y / (float)RC_TEX_SIZE;

            float n = noise_2d(fx * 3 + seed, fy * 3) * 0.1f;

            /* Grid lines every 32px */
            bool gridH = (y % 32 == 0 || y % 32 == 1);
            bool gridV = (x % 32 == 0 || x % 32 == 1);
            float grid = (gridH || gridV) ? 0.7f : 1.0f;

            int base = 65 + (int)(n * 20);
            out[y * RC_TEX_SIZE + x] = rgb(
                (int)(base * 0.9f * grid),
                (int)(base * 0.95f * grid),
                (int)(base * grid)
            );
        }
    }
}

static void gen_wall_bio(Color *out, unsigned int seed) {
    /* Organic, unsettling biological growth — dark green/purple */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float fx = x / (float)RC_TEX_SIZE;
            float fy = y / (float)RC_TEX_SIZE;

            float n = noise_fbm(fx * 5 + seed, fy * 5, 4, 2.2f, 0.6f);
            float veins = noise_fbm(fx * 12 + seed * 4, fy * 12 + 100, 3, 3.0f, 0.4f);
            veins = (veins < 0.3f) ? 0.4f : 1.0f;

            Color base = color_lerp(rgb(10, 25, 15), rgb(30, 50, 35), n);
            /* Purple highlights in crevices */
            if (n < 0.35f) base = color_lerp(base, rgb(40, 15, 45), 0.5f);

            out[y * RC_TEX_SIZE + x] = rgb(
                (int)(base.r * veins),
                (int)(base.g * veins),
                (int)(base.b * veins)
            );
        }
    }
}

static void gen_wall_dark(Color *out, unsigned int seed) {
    /* Nearly featureless dark wall — deepest corridors */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float fx = x / (float)RC_TEX_SIZE;
            float fy = y / (float)RC_TEX_SIZE;
            float n = noise_2d(fx * 4 + seed, fy * 4) * 0.15f;
            int v = 8 + (int)(n * 15);
            out[y * RC_TEX_SIZE + x] = rgb(v, v, v + 2);
        }
    }
}

static void gen_wall_door(Color *out, unsigned int seed) {
    /* Distinctive door — dark frame with lighter center panel */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            /* Frame (4px border) */
            bool isFrame = (x < 4 || x >= 60 || y < 4 || y >= 60);
            /* Handle area */
            bool isHandle = (x >= 48 && x <= 52 && y >= 28 && y <= 36);

            if (isFrame) {
                out[y * RC_TEX_SIZE + x] = rgb(25, 25, 30);
            } else if (isHandle) {
                out[y * RC_TEX_SIZE + x] = rgb(80, 75, 60); /* Brass handle */
            } else {
                float n = noise_2d(x * 0.1f + seed, y * 0.1f) * 0.1f;
                int v = 40 + (int)(n * 15);
                out[y * RC_TEX_SIZE + x] = rgb(v, v - 2, v - 4);
            }
        }
    }
}

static void gen_floor_tile(Color *out, unsigned int seed) {
    /* Industrial floor tiles */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float fx = x / (float)RC_TEX_SIZE;
            float fy = y / (float)RC_TEX_SIZE;

            float n = noise_fbm(fx * 4 + seed + 200, fy * 4, 3, 2.0f, 0.5f);

            /* Tile grid (every 32px) */
            bool grid = (x % 32 < 1 || y % 32 < 1);
            float gridF = grid ? 0.5f : 1.0f;

            int v = 22 + (int)(n * 20);
            out[y * RC_TEX_SIZE + x] = rgb(
                (int)(v * gridF),
                (int)(v * gridF),
                (int)((v + 2) * gridF)
            );
        }
    }
}

static void gen_floor_grate(Color *out, unsigned int seed) {
    /* Metal grating — alternating bars */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            bool bar = ((x + y) % 8 < 3);
            float n = noise_2d(x * 0.2f + seed, y * 0.2f) * 0.2f;

            if (bar) {
                int v = 35 + (int)(n * 20);
                out[y * RC_TEX_SIZE + x] = rgb(v, v - 2, v - 3);
            } else {
                out[y * RC_TEX_SIZE + x] = rgb(4, 4, 6); /* Void below */
            }
        }
    }
}

static void gen_ceil_panel(Color *out, unsigned int seed) {
    /* Ceiling panels — dark with occasional light strips */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float n = noise_2d(x * 0.15f + seed + 300, y * 0.15f) * 0.15f;

            /* Light strip every 32px */
            bool strip = (y % 32 >= 14 && y % 32 <= 18 && x > 8 && x < 56);

            if (strip) {
                out[y * RC_TEX_SIZE + x] = rgb(50, 55, 65); /* Dim fluorescent */
            } else {
                int v = 15 + (int)(n * 15);
                out[y * RC_TEX_SIZE + x] = rgb(v, v, v + 2);
            }
        }
    }
}

static void gen_ceil_vent(Color *out, unsigned int seed) {
    /* Ventilation ceiling — dark with slats */
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            bool slat = (y % 6 < 2);
            float n = noise_2d(x * 0.1f + seed + 400, y * 0.1f) * 0.1f;

            if (slat) {
                int v = 25 + (int)(n * 10);
                out[y * RC_TEX_SIZE + x] = rgb(v, v, v + 1);
            } else {
                out[y * RC_TEX_SIZE + x] = rgb(3, 3, 5);
            }
        }
    }
}

/* ── Generate all textures ────────────────────────────────────────── */
void texgen_generate_all(RaycastRenderer *r, unsigned int seed) {
    noise_seed(seed);

    /* Generate each texture type */
    gen_wall_metal(texStore[TEX_WALL_METAL], seed);
    gen_wall_concrete(texStore[TEX_WALL_CONCRETE], seed + 1);
    gen_wall_lab(texStore[TEX_WALL_LAB], seed + 2);
    gen_wall_bio(texStore[TEX_WALL_BIO], seed + 3);
    gen_wall_dark(texStore[TEX_WALL_DARK], seed + 4);
    gen_wall_door(texStore[TEX_WALL_DOOR], seed + 5);
    gen_floor_tile(texStore[TEX_FLOOR_TILE], seed + 6);
    gen_floor_grate(texStore[TEX_FLOOR_GRATE], seed + 7);
    gen_ceil_panel(texStore[TEX_CEIL_PANEL], seed + 8);
    gen_ceil_vent(texStore[TEX_CEIL_VENT], seed + 9);

    /* Register all with the renderer */
    for (int i = 0; i < TEX_COUNT; i++) {
        rc_add_texture(r, texStore[i]);
    }
}
