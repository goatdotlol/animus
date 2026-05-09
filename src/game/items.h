/*
 * EREBUS — Game Items & Objectives
 *
 * Memory Cores: collectible objectives scattered across the map.
 * Items: consumables the player can find and use.
 */
#ifndef GAME_ITEMS_H
#define GAME_ITEMS_H

#include "raylib.h"

#define MAX_ITEMS 32

typedef enum {
    ITEM_NONE,
    ITEM_MEMORY_CORE,   /* Objective — must collect to escape    */
    ITEM_BATTERY,       /* Restores flashlight battery           */
    ITEM_DECOY,         /* Throwable noise distraction           */
    ITEM_ADRENALINE,    /* Temporary sprint boost                */
    ITEM_KEY_CARD       /* Opens locked doors                    */
} ItemType;

typedef struct {
    ItemType type;
    float    x, y;       /* World position                       */
    bool     collected;
    bool     active;
    int      spriteId;   /* Registered with raycaster             */
    float    bobOffset;  /* Visual floating animation             */
} Item;

/* Player inventory */
typedef struct {
    int  batteries;
    int  decoys;
    int  adrenalines;
    int  keycards;
    int  memoryCores;
    int  totalCoresNeeded;
} Inventory;

struct Map;
struct RaycastRenderer;
struct Player;

/* Initialize items from map objective flags */
void items_init(Item items[], int *count, struct Map *map, struct RaycastRenderer *r);

/* Update item animations and check collection */
void items_update(Item items[], int count, struct Player *p, Inventory *inv, float dt);

/* Initialize inventory */
void inventory_init(Inventory *inv, int totalCores);

#endif /* GAME_ITEMS_H */
