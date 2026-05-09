/*
 * EREBUS — Monster AI Implementation
 */
#include "ai/monster.h"
#include "world/map.h"
#include "game/player.h"
#include "engine/raycaster.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

void monster_init(Monster *m, RaycastRenderer *r, float startX, float startY) {
    m->x = startX;
    m->y = startY;
    m->dirX = 1.0f;
    m->dirY = 0.0f;
    m->moveSpeed = 1.8f;
    m->state = MSTATE_WANDER;
    m->suspicion = 0.0f;
    
    nn_init(&m->brain, 12345); /* Base seed */

    /* Create a red placeholder sprite for the monster */
    Color *monsterTex = calloc(RC_TEX_SIZE * RC_TEX_SIZE, sizeof(Color));
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            /* Simple glowing red orb for now */
            float dx = x - RC_TEX_SIZE/2;
            float dy = y - RC_TEX_SIZE/2;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 20) {
                int glow = (int)(255 * (1.0f - dist/20.0f));
                monsterTex[y * RC_TEX_SIZE + x] = (Color){glow, 0, 0, 255};
            } else {
                monsterTex[y * RC_TEX_SIZE + x] = (Color){0, 0, 0, 0}; /* Transparent */
            }
        }
    }
    int texId = rc_add_texture(r, monsterTex);
    free(monsterTex);

    m->spriteId = rc_add_sprite(r, m->x, m->y, texId, 1.0f, WHITE);
}

/* Check line of sight using DDA (similar to raycaster) */
static bool check_los(const Map *map, float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist = sqrtf(dx*dx + dy*dy);
    
    if (dist > 15.0f) return false; /* Max vision distance */
    
    dx /= dist;
    dy /= dist;
    
    int mapX = (int)x1;
    int mapY = (int)y1;
    
    float deltaDistX = (dx == 0) ? 1e30f : fabsf(1.0f / dx);
    float deltaDistY = (dy == 0) ? 1e30f : fabsf(1.0f / dy);
    
    int stepX = (dx < 0) ? -1 : 1;
    int stepY = (dy < 0) ? -1 : 1;
    
    float sideDistX = (dx < 0) ? (x1 - mapX) * deltaDistX : (mapX + 1.0f - x1) * deltaDistX;
    float sideDistY = (dy < 0) ? (y1 - mapY) * deltaDistY : (mapY + 1.0f - y1) * deltaDistY;
    
    while (true) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
        }
        
        if (mapX == (int)x2 && mapY == (int)y2) return true;
        if (mapX < 0 || mapX >= map->width || mapY < 0 || mapY >= map->height) return false;
        if (map->cells[mapY][mapX].flags & CELL_SOLID) return false;
    }
}

void monster_update(Monster *m, const Map *map, const Player *p, float dt) {
    float distToPlayer = sqrtf((p->posX - m->x)*(p->posX - m->x) + (p->posY - m->y)*(p->posY - m->y));
    bool hasLos = check_los(map, m->x, m->y, p->posX, p->posY);
    
    /* ── Senses ───────────────────────────────────────────────────────── */
    /* Hearing */
    bool heardPlayer = false;
    if (distToPlayer <= p->noiseRadius) {
        heardPlayer = true;
    }
    
    /* ── NN Forward Pass (Thinking) ───────────────────────────────────── */
    /*
     * Inputs:
     * 0: Player Distance (normalized 0-1)
     * 1: Has Line of Sight
     * 2: Heard Player Noise
     * 3: Player Panic Input
     * 4: Map Light Level at Monster
     * 5: Time Hiding
     * 6: Time Sprinting
     * ... (remaining 19 inputs reserved for future features)
     */
    float inputs[NN_INPUTS] = {0};
    inputs[0] = fminf(distToPlayer / 20.0f, 1.0f);
    inputs[1] = hasLos ? 1.0f : 0.0f;
    inputs[2] = heardPlayer ? 1.0f : 0.0f;
    inputs[3] = p->panicInput;
    inputs[4] = map_get_light(map, m->x, m->y);
    inputs[5] = fminf(p->timeHiding / 60.0f, 1.0f);
    inputs[6] = fminf(p->timeSprinting / 60.0f, 1.0f);
    
    float outputs[NN_OUTPUTS] = {0};
    nn_forward(&m->brain, inputs, outputs);
    
    /* 
     * Outputs:
     * 0: Aggression (Speed multiplier)
     * 1: Chase probability threshold
     * 2: Investigate probability threshold
     */
    float aggression = outputs[0];
    float chaseThresh = outputs[1];
    
    /* ── State Machine ────────────────────────────────────────────────── */
    if (hasLos) {
        m->suspicion += dt * (1.0f + aggression);
        if (m->suspicion > 0.5f || aggression > chaseThresh) {
            m->state = MSTATE_CHASE;
            m->lastSeenPlayerX = p->posX;
            m->lastSeenPlayerY = p->posY;
        }
    } else {
        m->suspicion -= dt * 0.2f;
        if (m->suspicion < 0.0f) m->suspicion = 0.0f;
        
        if (m->state == MSTATE_CHASE && !hasLos) {
            m->state = MSTATE_INVESTIGATE;
            /* Recalculate path to last known location */
            path_find(map, (int)m->x, (int)m->y, (int)m->lastSeenPlayerX, (int)m->lastSeenPlayerY, &m->currentPath);
        }
    }
    
    if (heardPlayer && m->state != MSTATE_CHASE) {
        m->state = MSTATE_INVESTIGATE;
        m->lastSeenPlayerX = p->posX;
        m->lastSeenPlayerY = p->posY;
        path_find(map, (int)m->x, (int)m->y, (int)m->lastSeenPlayerX, (int)m->lastSeenPlayerY, &m->currentPath);
    }
    
    /* ── Movement ─────────────────────────────────────────────────────── */
    float currentSpeed = m->moveSpeed * (0.8f + aggression * 0.5f);
    if (m->state == MSTATE_CHASE) currentSpeed *= 1.5f;
    
    float targetX = m->x;
    float targetY = m->y;
    
    if (m->state == MSTATE_CHASE) {
        targetX = p->posX;
        targetY = p->posY;
    } else if (m->state == MSTATE_INVESTIGATE || m->state == MSTATE_WANDER) {
        if (m->currentPath.length > 0 && m->currentPath.current_index < m->currentPath.length) {
            targetX = m->currentPath.points[m->currentPath.current_index].x + 0.5f;
            targetY = m->currentPath.points[m->currentPath.current_index].y + 0.5f;
            
            float distToNode = sqrtf((targetX - m->x)*(targetX - m->x) + (targetY - m->y)*(targetY - m->y));
            if (distToNode < 0.2f) {
                m->currentPath.current_index++;
            }
        } else {
            /* Arrived or no path */
            if (m->state == MSTATE_INVESTIGATE) {
                m->state = MSTATE_WANDER;
            }
            
            if (m->state == MSTATE_WANDER && (rand() % 100) < 2) {
                /* Pick new random point */
                int rx = rand() % map->width;
                int ry = rand() % map->height;
                if (map_is_walkable(map, rx + 0.5f, ry + 0.5f)) {
                    path_find(map, (int)m->x, (int)m->y, rx, ry, &m->currentPath);
                }
            }
        }
    }
    
    float dx = targetX - m->x;
    float dy = targetY - m->y;
    float dist = sqrtf(dx*dx + dy*dy);
    
    if (dist > 0.01f) {
        m->dirX = dx / dist;
        m->dirY = dy / dist;
        
        float newX = m->x + m->dirX * currentSpeed * dt;
        float newY = m->y + m->dirY * currentSpeed * dt;
        
        /* Basic collision */
        if (map_is_walkable(map, newX, m->y)) m->x = newX;
        if (map_is_walkable(map, m->x, newY)) m->y = newY;
    }
}
