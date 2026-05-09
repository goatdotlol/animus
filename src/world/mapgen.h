/*
 * EREBUS — Procedural Map Generator
 *
 * BSP (Binary Space Partition) dungeon generation.
 * Creates a new facility layout every run with:
 *   - Corridors of varying width
 *   - Rooms (labs, storage, offices, maintenance)
 *   - Guaranteed path from spawn to exit
 *   - Objective placement in dead ends
 *   - Monster spawn far from player
 *   - Lighting variation per sector type
 */
#ifndef WORLD_MAPGEN_H
#define WORLD_MAPGEN_H

#include "world/map.h"

typedef struct {
    int x, y, w, h;
} Room;

#define MAPGEN_MAX_ROOMS 30

typedef struct {
    Room rooms[MAPGEN_MAX_ROOMS];
    int  roomCount;
} MapGenResult;

/* Generate a new procedural map. Seed controls the layout. */
void mapgen_generate(Map *map, unsigned int seed, int width, int height, MapGenResult *result);

#endif /* WORLD_MAPGEN_H */
