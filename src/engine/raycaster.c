/*
 * EREBUS — DDA Raycasting Engine Implementation
 *
 * This is the beating heart of the renderer. Every frame:
 *   1. For each screen column, cast a ray through the grid
 *   2. DDA-step until hitting a wall
 *   3. Draw a textured vertical strip (height based on distance)
 *   4. Fill remaining pixels with floor/ceiling casting
 *   5. Sort and render billboard sprites on top
 *   6. UpdateTexture → GPU, draw scaled to window
 */
#include "engine/raycaster.h"
#include "world/map.h"
#include "game/player.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────── */
static inline int   clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline void put_pixel(RaycastRenderer *r, int x, int y, Color c) {
    if (x >= 0 && x < RC_WIDTH && y >= 0 && y < RC_HEIGHT)
        r->buffer[y * RC_WIDTH + x] = c;
}

static inline Color shade_color(Color c, float factor) {
    return (Color){
        (unsigned char)(c.r * factor),
        (unsigned char)(c.g * factor),
        (unsigned char)(c.b * factor),
        c.a
    };
}

/* Mix two colors: result = a*(1-t) + b*t */
static inline Color mix_color(Color a, Color b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    return (Color){
        (unsigned char)(a.r * (1 - t) + b.r * t),
        (unsigned char)(a.g * (1 - t) + b.g * t),
        (unsigned char)(a.b * (1 - t) + b.b * t),
        255
    };
}

/* Apply fog to a color based on distance */
static inline Color apply_fog(Color c, Color fog, float dist, float density) {
    float fogFactor = 1.0f - expf(-density * dist * dist * 0.01f);
    return mix_color(c, fog, fogFactor);
}

/* ── Init / Free ──────────────────────────────────────────────────── */
void rc_init(RaycastRenderer *r) {
    memset(r, 0, sizeof(*r));

    r->buffer  = (Color *)calloc(RC_WIDTH * RC_HEIGHT, sizeof(Color));
    r->zbuffer = (float *)calloc(RC_WIDTH, sizeof(float));

    /* Create the CPU image and GPU texture for blitting */
    r->screenImg = GenImageColor(RC_WIDTH, RC_HEIGHT, BLACK);
    r->screenTex = LoadTextureFromImage(r->screenImg);

    /* Default atmosphere */
    r->fogColor   = (Color){ 2, 2, 8, 255 };
    r->fogDensity = 0.15f;
    r->barrelDistortion = 0.0f;

    r->textureCount = 0;
    r->spriteCount  = 0;
}

void rc_free(RaycastRenderer *r) {
    free(r->buffer);
    free(r->zbuffer);
    UnloadTexture(r->screenTex);
    UnloadImage(r->screenImg);
    for (int i = 0; i < r->textureCount; i++) {
        free(r->textures[i]);
    }
}

int rc_add_texture(RaycastRenderer *r, Color *pixels) {
    if (r->textureCount >= RC_MAX_TEXTURES) return -1;
    /* Make a copy so the caller can free theirs */
    Color *copy = (Color *)malloc(RC_TEX_SIZE * RC_TEX_SIZE * sizeof(Color));
    memcpy(copy, pixels, RC_TEX_SIZE * RC_TEX_SIZE * sizeof(Color));
    r->textures[r->textureCount] = copy;
    return r->textureCount++;
}

/* ── Sprite management ────────────────────────────────────────────── */
int rc_add_sprite(RaycastRenderer *r, float x, float y, int texId, float scale, Color tint) {
    if (r->spriteCount >= RC_MAX_SPRITES) return -1;
    int idx = r->spriteCount++;
    r->sprites[idx] = (RCSprite){ x, y, texId, scale, tint, true };
    return idx;
}

void rc_remove_sprite(RaycastRenderer *r, int index) {
    if (index >= 0 && index < r->spriteCount) {
        r->sprites[index].active = false;
    }
}

/* ── The DDA Raycaster ────────────────────────────────────────────── */
void rc_render(RaycastRenderer *r, const struct Map *map, const struct Player *player) {
    float posX  = player->posX;
    float posY  = player->posY;
    float dirX  = player->dirX;
    float dirY  = player->dirY;
    float plX   = player->planeX;
    float plY   = player->planeY;
    float pitch  = player->pitch;
    float bobOff = player->bobAmount;

    int halfH = RC_HEIGHT / 2;

    /* Clear buffer to ceiling/floor gradient */
    for (int y = 0; y < RC_HEIGHT; y++) {
        float t = (float)y / RC_HEIGHT;
        Color bg;
        if (y < halfH) {
            /* Ceiling — very dark blue-black */
            bg = (Color){ 3, 3, 8, 255 };
        } else {
            /* Floor — very dark gray */
            bg = (Color){ 5, 5, 5, 255 };
        }
        for (int x = 0; x < RC_WIDTH; x++) {
            r->buffer[y * RC_WIDTH + x] = bg;
        }
    }

    /* ── Cast walls ──────────────────────────────────────────────── */
    for (int x = 0; x < RC_WIDTH; x++) {
        /* Camera X: -1 (left) to +1 (right) */
        float cameraX = 2.0f * x / (float)RC_WIDTH - 1.0f;

        /* Ray direction */
        float rayDirX = dirX + plX * cameraX;
        float rayDirY = dirY + plY * cameraX;

        /* Grid position */
        int mapX = (int)posX;
        int mapY = (int)posY;

        /* Delta distance: how far along the ray to cross one grid cell */
        float deltaDistX = (rayDirX == 0) ? 1e30f : fabsf(1.0f / rayDirX);
        float deltaDistY = (rayDirY == 0) ? 1e30f : fabsf(1.0f / rayDirY);

        /* Step direction and initial side distance */
        int   stepX, stepY;
        float sideDistX, sideDistY;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0f - posX) * deltaDistX;
        }

        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0f - posY) * deltaDistY;
        }

        /* ── DDA loop ─────────────────────────────────────────────── */
        int  hit  = 0;
        int  side = 0;     /* 0 = NS wall, 1 = EW wall */
        int  maxSteps = 64;

        while (!hit && maxSteps-- > 0) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            /* Bounds check */
            if (mapX < 0 || mapX >= map->width || mapY < 0 || mapY >= map->height) {
                hit = 1;  /* Out of bounds = solid border */
                break;
            }

            /* Check if this cell is solid */
            if (map->cells[mapY][mapX].flags & CELL_SOLID) {
                hit = 1;
            }
        }

        /* ── Calculate perpendicular distance (no fisheye) ─────── */
        float perpDist;
        if (side == 0)
            perpDist = (sideDistX - deltaDistX);
        else
            perpDist = (sideDistY - deltaDistY);

        if (perpDist < 0.001f) perpDist = 0.001f;

        r->zbuffer[x] = perpDist;

        /* ── Wall strip height ─────────────────────────────────── */
        int lineHeight = (int)(RC_HEIGHT / perpDist);

        int pitchOffset = (int)(pitch * RC_HEIGHT * 0.5f + bobOff);
        int drawStart = -lineHeight / 2 + halfH + pitchOffset;
        int drawEnd   =  lineHeight / 2 + halfH + pitchOffset;

        int texDrawStart = drawStart;
        int texDrawEnd   = drawEnd;

        if (drawStart < 0) drawStart = 0;
        if (drawEnd >= RC_HEIGHT) drawEnd = RC_HEIGHT - 1;

        /* ── Determine wall texture ────────────────────────────── */
        int texIdx = 0;
        if (mapX >= 0 && mapX < map->width && mapY >= 0 && mapY < map->height) {
            const MapCell *cell = &map->cells[mapY][mapX];
            if (side == 0)
                texIdx = (stepX < 0) ? cell->wallTexW : cell->wallTexE;
            else
                texIdx = (stepY < 0) ? cell->wallTexN : cell->wallTexS;
        }

        /* ── Calculate texture X coordinate ────────────────────── */
        float wallX;
        if (side == 0)
            wallX = posY + perpDist * rayDirY;
        else
            wallX = posX + perpDist * rayDirX;
        wallX -= floorf(wallX);

        int texX = (int)(wallX * RC_TEX_SIZE);
        if (texX < 0) texX = 0;
        if (texX >= RC_TEX_SIZE) texX = RC_TEX_SIZE - 1;

        /* Mirror texture on one side for seamless appearance */
        if ((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0))
            texX = RC_TEX_SIZE - 1 - texX;

        /* ── Get sector light level ────────────────────────────── */
        float sectorLight = 1.0f;
        if (mapX >= 0 && mapX < map->width && mapY >= 0 && mapY < map->height) {
            sectorLight = map->cells[mapY][mapX].light;
        }

        /* Side shading: EW walls are darker for depth perception */
        float sideFactor = (side == 1) ? 0.7f : 1.0f;

        /* Distance-based darkening */
        float distFactor = 1.0f / (1.0f + perpDist * perpDist * 0.04f);

        float totalLight = sectorLight * sideFactor * distFactor;
        totalLight = clampf(totalLight, 0.02f, 1.0f);

        /* ── Draw textured wall strip ──────────────────────────── */
        Color *tex = (texIdx >= 0 && texIdx < r->textureCount) ?
                     r->textures[texIdx] : NULL;

        for (int y = drawStart; y <= drawEnd; y++) {
            /* Calculate texture Y */
            int d = (y - pitchOffset) * 256 - RC_HEIGHT * 128 + lineHeight * 128;
            int texY = ((d * RC_TEX_SIZE) / lineHeight) / 256;
            texY = clampi(texY, 0, RC_TEX_SIZE - 1);

            Color pixel;
            if (tex) {
                pixel = tex[texY * RC_TEX_SIZE + texX];
            } else {
                /* Fallback: dark gray */
                pixel = (Color){ 40, 40, 45, 255 };
            }

            pixel = shade_color(pixel, totalLight);
            pixel = apply_fog(pixel, r->fogColor, perpDist, r->fogDensity);
            put_pixel(r, x, y, pixel);
        }

        /* ── Floor and ceiling casting ─────────────────────────── */
        float floorXWall, floorYWall;
        if (side == 0 && rayDirX > 0) { floorXWall = (float)mapX; floorYWall = (float)mapY + wallX; }
        else if (side == 0 && rayDirX < 0) { floorXWall = (float)mapX + 1.0f; floorYWall = (float)mapY + wallX; }
        else if (side == 1 && rayDirY > 0) { floorXWall = (float)mapX + wallX; floorYWall = (float)mapY; }
        else { floorXWall = (float)mapX + wallX; floorYWall = (float)mapY + 1.0f; }

        for (int y = drawEnd + 1; y < RC_HEIGHT; y++) {
            float currentDist = RC_HEIGHT / (2.0f * (y - pitchOffset) - RC_HEIGHT);
            if (currentDist < 0.01f) continue;

            float weight = currentDist / perpDist;
            float floorX = weight * floorXWall + (1.0f - weight) * posX;
            float floorY = weight * floorYWall + (1.0f - weight) * posY;

            int fmx = (int)floorX;
            int fmy = (int)floorY;

            /* Floor texture */
            int floorTexIdx = TEX_FLOOR_TILE;
            float fLight = 1.0f;
            if (fmx >= 0 && fmx < map->width && fmy >= 0 && fmy < map->height) {
                floorTexIdx = map->cells[fmy][fmx].floorTex;
                fLight = map->cells[fmy][fmx].light;
            }

            int ftx = (int)(floorX * RC_TEX_SIZE) & (RC_TEX_SIZE - 1);
            int fty = (int)(floorY * RC_TEX_SIZE) & (RC_TEX_SIZE - 1);

            float fDistFactor = 1.0f / (1.0f + currentDist * currentDist * 0.06f);
            float fTotalLight = fLight * fDistFactor;
            fTotalLight = clampf(fTotalLight, 0.01f, 1.0f);

            Color *fTex = (floorTexIdx >= 0 && floorTexIdx < r->textureCount) ?
                          r->textures[floorTexIdx] : NULL;

            Color floorPixel;
            if (fTex)
                floorPixel = fTex[fty * RC_TEX_SIZE + ftx];
            else
                floorPixel = (Color){ 20, 20, 22, 255 };

            floorPixel = shade_color(floorPixel, fTotalLight);
            floorPixel = apply_fog(floorPixel, r->fogColor, currentDist, r->fogDensity);
            put_pixel(r, x, y, floorPixel);

            /* Ceiling (mirror of floor) */
            int ceilY = RC_HEIGHT - 1 - y + 2 * pitchOffset;
            if (ceilY >= 0 && ceilY < RC_HEIGHT) {
                int ceilTexIdx = TEX_CEIL_PANEL;
                float cLight = 1.0f;
                if (fmx >= 0 && fmx < map->width && fmy >= 0 && fmy < map->height) {
                    ceilTexIdx = map->cells[fmy][fmx].ceilTex;
                    cLight = map->cells[fmy][fmx].light * 0.6f; /* ceilings dimmer */
                }

                Color *cTex = (ceilTexIdx >= 0 && ceilTexIdx < r->textureCount) ?
                              r->textures[ceilTexIdx] : NULL;

                Color ceilPixel;
                if (cTex)
                    ceilPixel = cTex[fty * RC_TEX_SIZE + ftx];
                else
                    ceilPixel = (Color){ 10, 10, 15, 255 };

                ceilPixel = shade_color(ceilPixel, cLight * fDistFactor);
                ceilPixel = apply_fog(ceilPixel, r->fogColor, currentDist, r->fogDensity);
                put_pixel(r, x, ceilY, ceilPixel);
            }
        }
    }

    /* ── Sprite rendering (sorted by distance, back to front) ──── */
    /* Sort active sprites by distance (simple bubble sort, N is small) */
    typedef struct { int idx; float dist; } SpriteDist;
    SpriteDist order[RC_MAX_SPRITES];
    int nActive = 0;

    for (int i = 0; i < r->spriteCount; i++) {
        if (!r->sprites[i].active) continue;
        float dx = r->sprites[i].x - posX;
        float dy = r->sprites[i].y - posY;
        order[nActive].idx  = i;
        order[nActive].dist = dx * dx + dy * dy;
        nActive++;
    }

    /* Sort descending (farthest first) */
    for (int i = 0; i < nActive - 1; i++) {
        for (int j = i + 1; j < nActive; j++) {
            if (order[j].dist > order[i].dist) {
                SpriteDist tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    /* Draw each sprite */
    for (int i = 0; i < nActive; i++) {
        RCSprite *spr = &r->sprites[order[i].idx];

        /* Transform sprite position relative to camera */
        float sprX = spr->x - posX;
        float sprY = spr->y - posY;

        /* Inverse camera matrix to get screen position */
        float invDet = 1.0f / (plX * dirY - dirX * plY);
        float transformX = invDet * (dirY * sprX - dirX * sprY);
        float transformY = invDet * (-plY * sprX + plX * sprY);

        if (transformY <= 0.1f) continue; /* Behind camera */

        int pitchOffset = (int)(pitch * RC_HEIGHT * 0.5f + bobOff);
        int sprScreenX = (int)((RC_WIDTH / 2) * (1 + transformX / transformY));

        int sprHeight = abs((int)(RC_HEIGHT / transformY)) * (int)(spr->scale + 0.5f);
        if (sprHeight < 1) sprHeight = 1;
        if (sprHeight > RC_HEIGHT * 4) sprHeight = RC_HEIGHT * 4;

        int drawStartY = -sprHeight / 2 + halfH + pitchOffset;
        int drawEndY   =  sprHeight / 2 + halfH + pitchOffset;

        int sprWidth = sprHeight; /* Square sprites */
        int drawStartX = -sprWidth / 2 + sprScreenX;
        int drawEndX   =  sprWidth / 2 + sprScreenX;

        int texStartY = drawStartY;
        if (drawStartY < 0) drawStartY = 0;
        if (drawEndY >= RC_HEIGHT) drawEndY = RC_HEIGHT - 1;
        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= RC_WIDTH) drawEndX = RC_WIDTH - 1;

        Color *sTex = (spr->textureId >= 0 && spr->textureId < r->textureCount) ?
                      r->textures[spr->textureId] : NULL;

        /* Distance shading */
        float sDist = sqrtf(order[i].dist);
        float sLight = 1.0f / (1.0f + sDist * sDist * 0.04f);
        sLight = clampf(sLight, 0.05f, 1.0f);

        for (int sx = drawStartX; sx <= drawEndX; sx++) {
            /* Check zbuffer — only draw if in front of walls */
            if (sx < 0 || sx >= RC_WIDTH) continue;
            if (transformY >= r->zbuffer[sx]) continue;

            int texX = (int)((sx - (sprScreenX - sprWidth / 2)) * RC_TEX_SIZE / sprWidth);
            texX = clampi(texX, 0, RC_TEX_SIZE - 1);

            for (int sy = drawStartY; sy <= drawEndY; sy++) {
                int texY = (int)((sy - texStartY) * RC_TEX_SIZE / sprHeight);
                texY = clampi(texY, 0, RC_TEX_SIZE - 1);

                Color pixel;
                if (sTex)
                    pixel = sTex[texY * RC_TEX_SIZE + texX];
                else
                    pixel = spr->tint;

                /* Skip transparent pixels (magenta = transparent) */
                if (pixel.a < 128) continue;
                if (pixel.r > 240 && pixel.g < 15 && pixel.b > 240) continue;

                pixel = shade_color(pixel, sLight);
                pixel = apply_fog(pixel, r->fogColor, sDist, r->fogDensity);
                put_pixel(r, sx, sy, pixel);
            }
        }
    }
}

/* ── Present: push buffer to GPU and draw scaled to window ─────── */
void rc_present(RaycastRenderer *r) {
    /* Copy pixel buffer into the Image, then update the GPU texture */
    memcpy(r->screenImg.data, r->buffer, RC_WIDTH * RC_HEIGHT * sizeof(Color));
    UpdateTexture(r->screenTex, r->screenImg.data);

    /* Scale to fill the window, preserving aspect ratio */
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    float scaleX = (float)sw / RC_WIDTH;
    float scaleY = (float)sh / RC_HEIGHT;
    float scale  = (scaleX < scaleY) ? scaleX : scaleY;

    int drawW = (int)(RC_WIDTH  * scale);
    int drawH = (int)(RC_HEIGHT * scale);
    int offX  = (sw - drawW) / 2;
    int offY  = (sh - drawH) / 2;

    Rectangle src  = { 0, 0, (float)RC_WIDTH, (float)RC_HEIGHT };
    Rectangle dst  = { (float)offX, (float)offY, (float)drawW, (float)drawH };
    Vector2   orig = { 0, 0 };

    /* Nearest-neighbor scaling preserves pixel art crispness */
    DrawTexturePro(r->screenTex, src, dst, orig, 0.0f, WHITE);
}
