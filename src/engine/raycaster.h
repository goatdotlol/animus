/*
 * EREBUS — Raycasting Engine
 * DDA-based 2.5D renderer with textured walls, floor/ceiling casting,
 * sector-based lighting, and billboard sprite support.
 */
#ifndef ENGINE_RAYCASTER_H
#define ENGINE_RAYCASTER_H

#include "raylib.h"

/* ── Render Configuration ─────────────────────────────────────────── */
#define RC_WIDTH   384      /* Internal render width  (low-res for aesthetic) */
#define RC_HEIGHT  216      /* Internal render height (16:9)                  */
#define RC_TEX_SIZE 64      /* Wall texture size (64x64 pixels)              */
#define RC_MAX_TEXTURES 16  /* Maximum wall/floor/ceiling textures           */
#define RC_MAX_SPRITES  64  /* Maximum billboard sprites in scene            */
#define RC_FOV     66.0f    /* Field of view in degrees                      */

/* ── Sprite (billboard) ───────────────────────────────────────────── */
typedef struct {
    float x, y;             /* World position                                */
    int   textureId;        /* Sprite texture index                          */
    float scale;            /* Render scale multiplier                       */
    Color tint;             /* Color tint                                    */
    bool  active;           /* Is this sprite visible?                       */
} RCSprite;

/* ── Renderer State ───────────────────────────────────────────────── */
typedef struct {
    Color  *buffer;         /* CPU pixel buffer (RC_WIDTH * RC_HEIGHT)        */
    float  *zbuffer;        /* Depth buffer per column (RC_WIDTH)             */
    Texture2D screenTex;    /* GPU texture — updated from buffer each frame   */
    Image     screenImg;    /* CPU image backing the texture                  */

    /* Textures (walls, floors, ceilings) */
    Color  *textures[RC_MAX_TEXTURES];
    int     textureCount;

    /* Sprites */
    RCSprite sprites[RC_MAX_SPRITES];
    int      spriteCount;

    /* Fog / atmosphere */
    Color   fogColor;
    float   fogDensity;     /* 0 = no fog, 1 = very dense                    */

    /* CRT barrel distortion amount (0..1) */
    float   barrelDistortion;
} RaycastRenderer;

/* ── Public API ───────────────────────────────────────────────────── */
void rc_init(RaycastRenderer *r);
void rc_free(RaycastRenderer *r);

/* Register a 64x64 texture. Returns texture index. */
int  rc_add_texture(RaycastRenderer *r, Color *pixels);

/* Render the scene. Needs forward-declared types from map.h / player.h */
struct Map;
struct Player;
void rc_render(RaycastRenderer *r, const struct Map *map, const struct Player *player);

/* Blit the internal buffer to screen, scaled to fill the window. */
void rc_present(RaycastRenderer *r);

/* Add a sprite to the scene. Returns sprite index or -1 on failure. */
int  rc_add_sprite(RaycastRenderer *r, float x, float y, int texId, float scale, Color tint);
void rc_remove_sprite(RaycastRenderer *r, int index);

#endif /* ENGINE_RAYCASTER_H */
