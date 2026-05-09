/*
 * EREBUS — Sector-Based Map Implementation
 *
 * Provides map loading, cell queries, and a hand-crafted test level
 * that demonstrates mixed tight/open spaces, multiple texture types,
 * and varied lighting. This test map is 24x24 and features:
 *   - Tight winding corridors (horror)
 *   - Open lab rooms (tactical)
 *   - Dark storage areas
 *   - A bio-contaminated zone
 *   - Player spawn and monster spawn points
 */
#include "world/map.h"
#include <string.h>
#include <math.h>

void map_init(Map *map) {
    memset(map, 0, sizeof(*map));
    map->width  = 0;
    map->height = 0;
}

MapCell *map_get_cell(Map *map, int x, int y) {
    if (x < 0 || x >= map->width || y < 0 || y >= map->height) return NULL;
    return &map->cells[y][x];
}

bool map_is_walkable(const Map *map, float x, float y) {
    int mx = (int)x;
    int my = (int)y;
    if (mx < 0 || mx >= map->width || my < 0 || my >= map->height) return false;
    return !(map->cells[my][mx].flags & CELL_SOLID);
}

float map_get_light(const Map *map, float x, float y) {
    int mx = (int)x;
    int my = (int)y;
    if (mx < 0 || mx >= map->width || my < 0 || my >= map->height) return 0.0f;
    return map->cells[my][mx].light;
}

/* ── Helper to set a wall cell ────────────────────────────────────── */
static void set_wall(Map *map, int x, int y, int tex) {
    MapCell *c = &map->cells[y][x];
    c->flags = CELL_SOLID;
    c->wallTexN = c->wallTexS = c->wallTexE = c->wallTexW = tex;
    c->floorTex = TEX_FLOOR_TILE;
    c->ceilTex  = TEX_CEIL_PANEL;
    c->light    = 0.5f;
}

/* Set a floor cell (walkable) */
static void set_floor(Map *map, int x, int y, int floorTex, int ceilTex, float light) {
    MapCell *c = &map->cells[y][x];
    c->flags = 0;
    c->wallTexN = c->wallTexS = c->wallTexE = c->wallTexW = TEX_WALL_METAL;
    c->floorTex = floorTex;
    c->ceilTex  = ceilTex;
    c->light    = light;
}

/* ── Test Map: "Facility Block Alpha" ─────────────────────────────── */
/*
 * Legend (in the grid below):
 *   # = Metal wall       C = Concrete wall     L = Lab wall
 *   B = Bio wall         D = Dark wall         . = Floor (normal)
 *   o = Floor (bright)   d = Floor (dark)      g = Grate floor
 *   S = Player spawn     M = Monster spawn     X = Exit
 *   O = Objective
 */
void map_load_test(Map *map) {
    /* 24x24 map */
    static const char layout[24][25] = {
        "########################",  /* 0  */
        "#...##.......##........#",  /* 1  */
        "#.S.##..ooo..##..dddd..#",  /* 2  */
        "#...##..ooo..##..dddd..#",  /* 3  */
        "#...##..ooo..##..dddd..#",  /* 4  */
        "##.###..oOo..##..dMdd..#",  /* 5  */
        "#....#.......##........#",  /* 6  */
        "#....#.......##.####.###",  /* 7  */
        "#.O..........##.#gg#.#.#",  /* 8  */
        "#....#..######..#gg#...#",  /* 9  */
        "##.###..#....#..####...#",  /* 10 */
        "#.......#.O..#.........#",  /* 11 */
        "#.......#....#..####.###",  /* 12 */
        "###.#####....#..#......#",  /* 13 */
        "#.........####..#..##..#",  /* 14 */
        "#..####........X#..##..#",  /* 15 */
        "#..#..#..####.###......#",  /* 16 */
        "#..#..#..#.......##.####",  /* 17 */
        "#.....#..#..ooo..##....#",  /* 18 */
        "###.###..#..oOo..#.....#",  /* 19 */
        "#........#..ooo..#..####",  /* 20 */
        "#..####..#.......#.....#",  /* 21 */
        "#........########......#",  /* 22 */
        "########################",  /* 23 */
    };

    map->width  = 24;
    map->height = 24;
    map->objectiveCount = 0;

    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 24; x++) {
            char ch = layout[y][x];
            MapCell *c = &map->cells[y][x];

            /* Default values */
            c->floorHeight = 0.0f;
            c->ceilHeight  = 1.0f;

            switch (ch) {
                case '#':
                    /* Vary wall types by region */
                    if (y < 8 && x > 12)
                        set_wall(map, x, y, TEX_WALL_CONCRETE);
                    else if (y >= 16 || (x >= 7 && x <= 14 && y >= 8 && y <= 14))
                        set_wall(map, x, y, TEX_WALL_LAB);
                    else
                        set_wall(map, x, y, TEX_WALL_METAL);
                    break;

                case '.':
                    set_floor(map, x, y, TEX_FLOOR_TILE, TEX_CEIL_PANEL, 0.4f);
                    break;

                case 'o': /* Bright lab floor */
                    set_floor(map, x, y, TEX_FLOOR_TILE, TEX_CEIL_PANEL, 0.8f);
                    break;

                case 'd': /* Dark area */
                    set_floor(map, x, y, TEX_FLOOR_TILE, TEX_CEIL_VENT, 0.12f);
                    break;

                case 'g': /* Grate floor */
                    set_floor(map, x, y, TEX_FLOOR_GRATE, TEX_CEIL_VENT, 0.25f);
                    break;

                case 'S': /* Player spawn */
                    set_floor(map, x, y, TEX_FLOOR_TILE, TEX_CEIL_PANEL, 0.5f);
                    c->flags |= CELL_SPAWN;
                    map->spawnX = x + 0.5f;
                    map->spawnY = y + 0.5f;
                    map->spawnAngle = 0.0f;
                    break;

                case 'M': /* Monster spawn */
                    set_floor(map, x, y, TEX_FLOOR_TILE, TEX_CEIL_VENT, 0.1f);
                    c->flags |= CELL_MONSTER;
                    map->monsterX = x + 0.5f;
                    map->monsterY = y + 0.5f;
                    break;

                case 'X': /* Exit */
                    set_floor(map, x, y, TEX_FLOOR_GRATE, TEX_CEIL_PANEL, 0.6f);
                    c->flags |= CELL_EXIT;
                    /* Door texture on surrounding walls */
                    break;

                case 'O': /* Objective (memory core) */
                    set_floor(map, x, y, TEX_FLOOR_TILE, TEX_CEIL_PANEL, 0.7f);
                    c->flags |= CELL_OBJECTIVE;
                    map->objectiveCount++;
                    break;

                default:
                    set_floor(map, x, y, TEX_FLOOR_TILE, TEX_CEIL_PANEL, 0.3f);
                    break;
            }
        }
    }
}
