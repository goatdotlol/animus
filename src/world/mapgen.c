/*
 * EREBUS — Procedural Map Generator Implementation
 *
 * Uses BSP subdivision to carve rooms, then connects them
 * with corridors. Places objectives in dead-end rooms and
 * ensures spawn/exit are maximally separated.
 */
#include "world/mapgen.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIN_ROOM_SIZE 4
#define MAX_SPLIT_DEPTH 5

typedef struct BSPNode {
    int x, y, w, h;
    int roomX, roomY, roomW, roomH;
    int hasRoom;
    struct BSPNode *left;
    struct BSPNode *right;
} BSPNode;

static BSPNode nodePool[128];
static int nodeCount = 0;

static BSPNode *alloc_node(int x, int y, int w, int h) {
    BSPNode *n = &nodePool[nodeCount++];
    n->x = x; n->y = y; n->w = w; n->h = h;
    n->hasRoom = 0;
    n->left = NULL; n->right = NULL;
    return n;
}

static void bsp_split(BSPNode *node, int depth) {
    if (depth >= MAX_SPLIT_DEPTH || node->w < MIN_ROOM_SIZE * 2 + 3 ||
        node->h < MIN_ROOM_SIZE * 2 + 3) {
        return;
    }

    int splitH = (rand() % 2 == 0);
    if (node->w > node->h * 1.5f) splitH = 0;
    if (node->h > node->w * 1.5f) splitH = 1;

    if (splitH) {
        int split = MIN_ROOM_SIZE + rand() % (node->h - MIN_ROOM_SIZE * 2);
        node->left  = alloc_node(node->x, node->y, node->w, split);
        node->right = alloc_node(node->x, node->y + split, node->w, node->h - split);
    } else {
        int split = MIN_ROOM_SIZE + rand() % (node->w - MIN_ROOM_SIZE * 2);
        node->left  = alloc_node(node->x, node->y, split, node->h);
        node->right = alloc_node(node->x + split, node->y, node->w - split, node->h);
    }

    bsp_split(node->left, depth + 1);
    bsp_split(node->right, depth + 1);
}

static void carve_room(Map *map, BSPNode *node, Room rooms[], int *roomCount) {
    if (node->left || node->right) {
        if (node->left)  carve_room(map, node->left, rooms, roomCount);
        if (node->right) carve_room(map, node->right, rooms, roomCount);
        return;
    }

    /* Leaf node — create a room */
    int rw = MIN_ROOM_SIZE + rand() % (node->w - MIN_ROOM_SIZE - 1);
    int rh = MIN_ROOM_SIZE + rand() % (node->h - MIN_ROOM_SIZE - 1);
    if (rw > node->w - 2) rw = node->w - 2;
    if (rh > node->h - 2) rh = node->h - 2;
    int rx = node->x + 1 + rand() % (node->w - rw - 1);
    int ry = node->y + 1 + rand() % (node->h - rh - 1);

    node->roomX = rx; node->roomY = ry;
    node->roomW = rw; node->roomH = rh;
    node->hasRoom = 1;

    /* Room type based on size */
    int roomType = rand() % 5;
    float light = 0.3f + (float)(rand() % 40) / 100.0f;
    int wallTex = (roomType < 2) ? TEX_LAB_TILE : (roomType < 4) ? TEX_CONCRETE : TEX_METAL_PANEL;
    int floorTex = TEX_CONCRETE;
    int ceilTex = TEX_METAL_PANEL;

    for (int y = ry; y < ry + rh; y++) {
        for (int x = rx; x < rx + rw; x++) {
            if (x >= 0 && x < map->width && y >= 0 && y < map->height) {
                map->cells[y][x].flags = 0; /* Clear solid */
                map->cells[y][x].light = light;
                map->cells[y][x].floorTex = floorTex;
                map->cells[y][x].ceilTex = ceilTex;
                map->cells[y][x].wallTexN = wallTex;
                map->cells[y][x].wallTexS = wallTex;
                map->cells[y][x].wallTexE = wallTex;
                map->cells[y][x].wallTexW = wallTex;
            }
        }
    }

    if (*roomCount < MAPGEN_MAX_ROOMS) {
        rooms[*roomCount].x = rx;
        rooms[*roomCount].y = ry;
        rooms[*roomCount].w = rw;
        rooms[*roomCount].h = rh;
        (*roomCount)++;
    }
}

static void carve_corridor_h(Map *map, int x1, int x2, int y) {
    int start = (x1 < x2) ? x1 : x2;
    int end   = (x1 < x2) ? x2 : x1;
    for (int x = start; x <= end; x++) {
        if (x >= 0 && x < map->width && y >= 0 && y < map->height) {
            map->cells[y][x].flags = 0;
            map->cells[y][x].light = 0.15f;
            map->cells[y][x].floorTex = TEX_CONCRETE;
            map->cells[y][x].wallTexN = TEX_METAL_PANEL;
            map->cells[y][x].wallTexS = TEX_METAL_PANEL;
            map->cells[y][x].wallTexE = TEX_METAL_PANEL;
            map->cells[y][x].wallTexW = TEX_METAL_PANEL;
        }
    }
}

static void carve_corridor_v(Map *map, int y1, int y2, int x) {
    int start = (y1 < y2) ? y1 : y2;
    int end   = (y1 < y2) ? y2 : y1;
    for (int y = start; y <= end; y++) {
        if (x >= 0 && x < map->width && y >= 0 && y < map->height) {
            map->cells[y][x].flags = 0;
            map->cells[y][x].light = 0.15f;
            map->cells[y][x].floorTex = TEX_CONCRETE;
            map->cells[y][x].wallTexN = TEX_METAL_PANEL;
            map->cells[y][x].wallTexS = TEX_METAL_PANEL;
            map->cells[y][x].wallTexE = TEX_METAL_PANEL;
            map->cells[y][x].wallTexW = TEX_METAL_PANEL;
        }
    }
}

static void get_room_center(BSPNode *node, int *cx, int *cy) {
    if (node->hasRoom) {
        *cx = node->roomX + node->roomW / 2;
        *cy = node->roomY + node->roomH / 2;
        return;
    }
    if (node->left) { get_room_center(node->left, cx, cy); return; }
    if (node->right) { get_room_center(node->right, cx, cy); return; }
    *cx = node->x + node->w / 2;
    *cy = node->y + node->h / 2;
}

static void connect_rooms(Map *map, BSPNode *node) {
    if (!node->left || !node->right) return;

    connect_rooms(map, node->left);
    connect_rooms(map, node->right);

    int cx1, cy1, cx2, cy2;
    get_room_center(node->left, &cx1, &cy1);
    get_room_center(node->right, &cx2, &cy2);

    /* L-shaped corridor */
    if (rand() % 2 == 0) {
        carve_corridor_h(map, cx1, cx2, cy1);
        carve_corridor_v(map, cy1, cy2, cx2);
    } else {
        carve_corridor_v(map, cy1, cy2, cx1);
        carve_corridor_h(map, cx1, cx2, cy2);
    }
}

void mapgen_generate(Map *map, unsigned int seed, int width, int height, MapGenResult *result) {
    srand(seed);
    nodeCount = 0;
    result->roomCount = 0;

    if (width > MAP_MAX_SIZE) width = MAP_MAX_SIZE;
    if (height > MAP_MAX_SIZE) height = MAP_MAX_SIZE;

    map->width = width;
    map->height = height;

    /* Fill everything solid */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            map->cells[y][x].flags = CELL_SOLID;
            map->cells[y][x].wallTexN = TEX_CONCRETE;
            map->cells[y][x].wallTexS = TEX_CONCRETE;
            map->cells[y][x].wallTexE = TEX_CONCRETE;
            map->cells[y][x].wallTexW = TEX_CONCRETE;
            map->cells[y][x].floorTex = TEX_CONCRETE;
            map->cells[y][x].ceilTex = TEX_METAL_PANEL;
            map->cells[y][x].light = 0.0f;
            map->cells[y][x].ceilHeight = 1.0f;
            map->cells[y][x].floorHeight = 0.0f;
        }
    }

    /* BSP subdivision */
    BSPNode *root = alloc_node(1, 1, width - 2, height - 2);
    bsp_split(root, 0);

    /* Carve rooms */
    carve_room(map, root, result->rooms, &result->roomCount);

    /* Connect rooms with corridors */
    connect_rooms(map, root);

    /* Place spawn in first room, exit in furthest room */
    if (result->roomCount >= 2) {
        Room *spawnRoom = &result->rooms[0];
        map->spawnX = spawnRoom->x + spawnRoom->w / 2.0f + 0.5f;
        map->spawnY = spawnRoom->y + spawnRoom->h / 2.0f + 0.5f;
        map->spawnAngle = 0;

        int spawnCx = spawnRoom->x + spawnRoom->w / 2;
        int spawnCy = spawnRoom->y + spawnRoom->h / 2;
        map->cells[spawnCy][spawnCx].flags |= CELL_SPAWN;

        /* Find furthest room for exit */
        float maxDist = 0;
        int exitIdx = result->roomCount - 1;
        for (int i = 1; i < result->roomCount; i++) {
            float dx = (float)(result->rooms[i].x - spawnRoom->x);
            float dy = (float)(result->rooms[i].y - spawnRoom->y);
            float d = sqrtf(dx * dx + dy * dy);
            if (d > maxDist) { maxDist = d; exitIdx = i; }
        }

        Room *exitRoom = &result->rooms[exitIdx];
        int exitCx = exitRoom->x + exitRoom->w / 2;
        int exitCy = exitRoom->y + exitRoom->h / 2;
        map->cells[exitCy][exitCx].flags |= CELL_EXIT;
        map->cells[exitCy][exitCx].light = 0.8f; /* Bright exit */

        /* Monster spawns at second-furthest room */
        float secondMax = 0;
        int monsterIdx = 1;
        for (int i = 1; i < result->roomCount; i++) {
            if (i == exitIdx) continue;
            float dx = (float)(result->rooms[i].x - spawnRoom->x);
            float dy = (float)(result->rooms[i].y - spawnRoom->y);
            float d = sqrtf(dx * dx + dy * dy);
            if (d > secondMax) { secondMax = d; monsterIdx = i; }
        }
        Room *monsterRoom = &result->rooms[monsterIdx];
        map->monsterX = monsterRoom->x + monsterRoom->w / 2.0f + 0.5f;
        map->monsterY = monsterRoom->y + monsterRoom->h / 2.0f + 0.5f;
        map->cells[(int)map->monsterY][(int)map->monsterX].flags |= CELL_MONSTER;

        /* Place objectives in small/dead-end rooms */
        map->objectiveCount = 0;
        for (int i = 1; i < result->roomCount; i++) {
            if (i == exitIdx || i == monsterIdx) continue;
            if (map->objectiveCount >= 4) break;
            Room *r = &result->rooms[i];
            int cx = r->x + r->w / 2;
            int cy = r->y + r->h / 2;
            map->cells[cy][cx].flags |= CELL_OBJECTIVE;
            map->objectiveCount++;
        }
        /* Guarantee at least 3 objectives */
        if (map->objectiveCount < 3) map->objectiveCount = 3;
    }
}
