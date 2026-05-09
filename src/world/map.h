/*
 * EREBUS — Sector-Based Map System
 * Grid-based map where each cell stores wall types, light level,
 * floor/ceiling textures, and sector metadata.
 */
#ifndef WORLD_MAP_H
#define WORLD_MAP_H

#include "raylib.h"
#include "engine/texgen.h"

#define MAP_MAX_SIZE  64    /* Maximum map dimension (64x64)               */

/* ── Cell flags ───────────────────────────────────────────────────── */
#define CELL_SOLID     (1 << 0)  /* Wall — blocks movement and rays       */
#define CELL_DOOR      (1 << 1)  /* Door — can be opened/closed           */
#define CELL_OBJECTIVE (1 << 2)  /* Contains a memory core                */
#define CELL_SPAWN     (1 << 3)  /* Player spawn point                    */
#define CELL_EXIT      (1 << 4)  /* Level exit                            */
#define CELL_MONSTER   (1 << 5)  /* Monster spawn point                   */

/* ── Map Cell ─────────────────────────────────────────────────────── */
typedef struct {
    int   wallTexN, wallTexS, wallTexE, wallTexW;  /* Wall texture per side  */
    int   floorTex;                                 /* Floor texture index    */
    int   ceilTex;                                  /* Ceiling texture index  */
    float light;                                    /* Sector light 0..1     */
    int   flags;                                    /* CELL_* flags           */
    float ceilHeight;                               /* Ceiling height (1.0 default) */
    float floorHeight;                              /* Floor height (0.0 default)   */
} MapCell;

/* ── Map ──────────────────────────────────────────────────────────── */
typedef struct Map {
    int     width, height;
    MapCell cells[MAP_MAX_SIZE][MAP_MAX_SIZE];

    /* Spawn positions extracted from cell flags */
    float   spawnX, spawnY;
    float   spawnAngle;
    float   monsterX, monsterY;
    int     objectiveCount;
} Map;

/* ── Public API ───────────────────────────────────────────────────── */
void map_init(Map *map);

/* Load the hand-crafted test map (Sprint 1) */
void map_load_test(Map *map);

/* Query a cell safely (returns NULL if out of bounds) */
MapCell *map_get_cell(Map *map, int x, int y);

/* Check if a position is walkable */
bool map_is_walkable(const Map *map, float x, float y);

/* Get the light level at a world position */
float map_get_light(const Map *map, float x, float y);

#endif /* WORLD_MAP_H */
