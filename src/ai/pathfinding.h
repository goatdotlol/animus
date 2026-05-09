/*
 * EREBUS — A* Pathfinding
 *
 * Calculates the shortest path on the sector grid.
 */
#ifndef AI_PATHFINDING_H
#define AI_PATHFINDING_H

#include <stdbool.h>

#define MAX_PATH_LENGTH 128

typedef struct {
    int x, y;
} Int2;

typedef struct {
    Int2 points[MAX_PATH_LENGTH];
    int length;
    int current_index;
} Path;

struct Map;

/* Find path from start to goal. Returns true if path found. */
bool path_find(const struct Map *map, int startX, int startY, int goalX, int goalY, Path *outPath);

#endif /* AI_PATHFINDING_H */
