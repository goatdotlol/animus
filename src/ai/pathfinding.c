/*
 * EREBUS — A* Pathfinding Implementation
 */
#include "ai/pathfinding.h"
#include "world/map.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int x, y;
    int gCost;
    int hCost;
    int fCost;
    int parentX, parentY;
    bool closed;
    bool open;
} Node;

static int heuristic(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

bool path_find(const Map *map, int startX, int startY, int goalX, int goalY, Path *outPath) {
    outPath->length = 0;
    outPath->current_index = 0;

    if (startX == goalX && startY == goalY) return true;
    if (startX < 0 || startX >= map->width || startY < 0 || startY >= map->height) return false;
    if (goalX < 0 || goalX >= map->width || goalY < 0 || goalY >= map->height) return false;
    if (!map_is_walkable(map, goalX + 0.5f, goalY + 0.5f)) return false;

    Node *nodes = calloc(map->width * map->height, sizeof(Node));
    if (!nodes) return false;

    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            nodes[y * map->width + x].x = x;
            nodes[y * map->width + x].y = y;
        }
    }

    int openCount = 0;
    Node *startNode = &nodes[startY * map->width + startX];
    startNode->gCost = 0;
    startNode->hCost = heuristic(startX, startY, goalX, goalY);
    startNode->fCost = startNode->gCost + startNode->hCost;
    startNode->open = true;
    openCount++;

    bool found = false;

    while (openCount > 0) {
        /* Find lowest fCost node in open list */
        Node *current = NULL;
        int lowestF = 9999999;
        
        for (int i = 0; i < map->width * map->height; i++) {
            if (nodes[i].open && nodes[i].fCost < lowestF) {
                lowestF = nodes[i].fCost;
                current = &nodes[i];
            }
        }

        if (!current) break;

        /* Reached goal? */
        if (current->x == goalX && current->y == goalY) {
            found = true;
            break;
        }

        current->open = false;
        current->closed = true;
        openCount--;

        /* Check neighbors */
        int dx[4] = {0, 1, 0, -1};
        int dy[4] = {-1, 0, 1, 0};

        for (int i = 0; i < 4; i++) {
            int nx = current->x + dx[i];
            int ny = current->y + dy[i];

            if (nx < 0 || nx >= map->width || ny < 0 || ny >= map->height) continue;
            if (!map_is_walkable(map, nx + 0.5f, ny + 0.5f)) continue;

            Node *neighbor = &nodes[ny * map->width + nx];
            if (neighbor->closed) continue;

            int newCost = current->gCost + 10;
            
            if (!neighbor->open || newCost < neighbor->gCost) {
                neighbor->gCost = newCost;
                neighbor->hCost = heuristic(nx, ny, goalX, goalY) * 10;
                neighbor->fCost = neighbor->gCost + neighbor->hCost;
                neighbor->parentX = current->x;
                neighbor->parentY = current->y;
                
                if (!neighbor->open) {
                    neighbor->open = true;
                    openCount++;
                }
            }
        }
    }

    if (found) {
        /* Reconstruct path */
        Node *current = &nodes[goalY * map->width + goalX];
        Int2 tempPath[MAX_PATH_LENGTH];
        int count = 0;
        
        while ((current->x != startX || current->y != startY) && count < MAX_PATH_LENGTH) {
            tempPath[count].x = current->x;
            tempPath[count].y = current->y;
            count++;
            current = &nodes[current->parentY * map->width + current->parentX];
        }

        /* Reverse into output */
        outPath->length = count;
        for (int i = 0; i < count; i++) {
            outPath->points[i] = tempPath[count - 1 - i];
        }
    }

    free(nodes);
    return found;
}
