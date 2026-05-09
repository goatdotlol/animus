/*
 * EREBUS — Items & Objectives Implementation
 */
#include "game/items.h"
#include "world/map.h"
#include "game/player.h"
#include "engine/raycaster.h"
#include <stdlib.h>
#include <math.h>

/* Generate a glowing item sprite texture */
static Color *gen_item_texture(Color baseColor) {
    Color *tex = calloc(RC_TEX_SIZE * RC_TEX_SIZE, sizeof(Color));
    int cx = RC_TEX_SIZE / 2, cy = RC_TEX_SIZE / 2;
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < 14) {
                float t = 1.0f - dist / 14.0f;
                tex[y * RC_TEX_SIZE + x] = (Color){
                    (unsigned char)(baseColor.r * t),
                    (unsigned char)(baseColor.g * t),
                    (unsigned char)(baseColor.b * t),
                    255
                };
            } else {
                tex[y * RC_TEX_SIZE + x] = (Color){0, 0, 0, 0};
            }
        }
    }
    return tex;
}

void inventory_init(Inventory *inv, int totalCores) {
    inv->batteries = 0;
    inv->decoys = 0;
    inv->adrenalines = 0;
    inv->keycards = 0;
    inv->memoryCores = 0;
    inv->totalCoresNeeded = totalCores;
}

void items_init(Item items[], int *count, Map *map, RaycastRenderer *r) {
    *count = 0;

    /* Create item textures */
    Color *coreTex = gen_item_texture((Color){0, 255, 200, 255});
    int coreTexId = rc_add_texture(r, coreTex);
    free(coreTex);

    Color *batteryTex = gen_item_texture((Color){255, 255, 80, 255});
    int batteryTexId = rc_add_texture(r, batteryTex);
    free(batteryTex);

    /* Spawn memory cores at objective locations */
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            if (map->cells[y][x].flags & CELL_OBJECTIVE) {
                if (*count >= MAX_ITEMS) break;
                Item *item = &items[*count];
                item->type = ITEM_MEMORY_CORE;
                item->x = x + 0.5f;
                item->y = y + 0.5f;
                item->collected = false;
                item->active = true;
                item->bobOffset = (float)(rand() % 100) / 100.0f * 6.28f;
                item->spriteId = rc_add_sprite(r, item->x, item->y, coreTexId, 0.6f, WHITE);
                (*count)++;
            }
        }
    }

    /* Scatter some batteries in dark areas */
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            if (!(map->cells[y][x].flags & CELL_SOLID) &&
                map->cells[y][x].light < 0.3f &&
                !(map->cells[y][x].flags & CELL_OBJECTIVE) &&
                (rand() % 100) < 8 &&
                *count < MAX_ITEMS) {
                Item *item = &items[*count];
                item->type = ITEM_BATTERY;
                item->x = x + 0.5f;
                item->y = y + 0.5f;
                item->collected = false;
                item->active = true;
                item->bobOffset = (float)(rand() % 100) / 100.0f * 6.28f;
                item->spriteId = rc_add_sprite(r, item->x, item->y, batteryTexId, 0.4f, WHITE);
                (*count)++;
            }
        }
    }
}

void items_update(Item items[], int count, Player *p, Inventory *inv, float dt) {
    for (int i = 0; i < count; i++) {
        Item *item = &items[i];
        if (!item->active || item->collected) continue;

        /* Floating bob animation */
        item->bobOffset += dt * 2.0f;

        /* Check pickup distance */
        float dx = p->posX - item->x;
        float dy = p->posY - item->y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < 0.6f) {
            item->collected = true;
            item->active = false;

            switch (item->type) {
                case ITEM_MEMORY_CORE:
                    inv->memoryCores++;
                    break;
                case ITEM_BATTERY:
                    inv->batteries++;
                    p->flashlightBattery += 0.3f;
                    if (p->flashlightBattery > 1.0f) p->flashlightBattery = 1.0f;
                    break;
                case ITEM_DECOY:
                    inv->decoys++;
                    break;
                case ITEM_ADRENALINE:
                    inv->adrenalines++;
                    break;
                case ITEM_KEY_CARD:
                    inv->keycards++;
                    break;
                default:
                    break;
            }

            /* Hide the sprite */
            if (item->spriteId >= 0) {
                /* We can't easily remove sprites, so just move it far away */
                /* A proper implementation would use rc_remove_sprite */
            }
        }
    }
}
