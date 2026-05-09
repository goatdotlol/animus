/*
 * EREBUS — Monster AI Implementation (v2: Advanced Behaviors)
 *
 * New systems:
 *   - Menace Gauge: The monster builds tension by stalking before attacking.
 *     It will intentionally "lose" the player to create false security.
 *   - Environmental manipulation: Turns off lights near corridors the
 *     player uses, creating darker paths.
 *   - Overfitting detection: If the NN outputs become too repetitive,
 *     the monster enters a "glitch" state with erratic behavior.
 */
#include "ai/monster.h"
#include "world/map.h"
#include "game/player.h"
#include "engine/raycaster.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

void monster_init(Monster *m, struct RaycastRenderer *r, float startX, float startY) {
    m->x = startX;
    m->y = startY;
    m->dirX = 1.0f;
    m->dirY = 0.0f;
    m->moveSpeed = 1.8f;
    m->state = MSTATE_WANDER;
    m->suspicion = 0.0f;

    /* Menace gauge */
    m->menace.tension = 0.0f;
    m->menace.mercyTimer = 0.0f;
    m->menace.nearMissCount = 0;
    m->menace.isFakingMercy = false;
    m->menace.dramaPause = 0.0f;

    /* Environmental */
    m->envTimer = 0.0f;
    m->lightsKilled = 0;

    /* Overfitting detection */
    m->patternScore = 0.0f;
    m->sameOutputCount = 0;
    m->glitchTimer = 0.0f;
    for (int i = 0; i < NN_OUTPUTS; i++) m->lastOutputs[i] = 0;

    nn_init(&m->brain, 12345);

    /* Create a red placeholder sprite for the monster */
    Color *monsterTex = calloc(RC_TEX_SIZE * RC_TEX_SIZE, sizeof(Color));
    for (int y = 0; y < RC_TEX_SIZE; y++) {
        for (int x = 0; x < RC_TEX_SIZE; x++) {
            float dx = (float)x - RC_TEX_SIZE/2;
            float dy = (float)y - RC_TEX_SIZE/2;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 20) {
                int glow = (int)(255 * (1.0f - dist/20.0f));
                monsterTex[y * RC_TEX_SIZE + x] = (Color){(unsigned char)glow, 0, 0, 255};
            } else {
                monsterTex[y * RC_TEX_SIZE + x] = (Color){0, 0, 0, 0};
            }
        }
    }
    int texId = rc_add_texture(r, monsterTex);
    free(monsterTex);

    m->spriteId = rc_add_sprite(r, m->x, m->y, texId, 1.0f, WHITE);
}

/* ── Line-of-Sight (DDA) ─────────────────────────────────────────── */
static bool check_los(const Map *map, float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist = sqrtf(dx*dx + dy*dy);

    if (dist > 15.0f) return false;

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

/* ── Menace Gauge Update ─────────────────────────────────────────── */
static void menace_update(MenaceGauge *mg, float distToPlayer, bool hasLos, float dt) {
    /* Build tension when close but not chasing */
    if (distToPlayer < 10.0f) {
        mg->tension += dt * 0.1f * (10.0f - distToPlayer) / 10.0f;
    } else {
        mg->tension -= dt * 0.03f;
    }
    if (mg->tension < 0) mg->tension = 0;
    if (mg->tension > 1) mg->tension = 1;

    /* Mercy cooldown */
    if (mg->mercyTimer > 0) {
        mg->mercyTimer -= dt;
        if (mg->mercyTimer <= 0) {
            mg->isFakingMercy = false;
        }
    }

    /* Drama pause countdown */
    if (mg->dramaPause > 0) {
        mg->dramaPause -= dt;
    }

    /* Decide to fake mercy: if tension is high and we just spotted them,
     * sometimes pretend to lose them to let them relax before the real chase */
    if (hasLos && distToPlayer < 5.0f && !mg->isFakingMercy && mg->nearMissCount < 3) {
        if ((rand() % 100) < 20) { /* 20% chance to fake mercy */
            mg->isFakingMercy = true;
            mg->mercyTimer = 3.0f + (float)(rand() % 30) / 10.0f; /* 3-6 seconds of fake mercy */
            mg->nearMissCount++;
        }
    }
}

/* ── Overfitting Detection ───────────────────────────────────────── */
static bool check_overfitting(Monster *m, const float outputs[NN_OUTPUTS]) {
    /* Compare current outputs to previous outputs */
    float similarity = 0;
    for (int i = 0; i < NN_OUTPUTS; i++) {
        float diff = fabsf(outputs[i] - m->lastOutputs[i]);
        similarity += (diff < 0.05f) ? 1.0f : 0.0f;
    }
    similarity /= NN_OUTPUTS;

    /* Copy current outputs for next comparison */
    for (int i = 0; i < NN_OUTPUTS; i++) {
        m->lastOutputs[i] = outputs[i];
    }

    if (similarity > 0.85f) {
        m->sameOutputCount++;
    } else {
        m->sameOutputCount = 0;
    }

    m->patternScore = (float)m->sameOutputCount / 50.0f;
    if (m->patternScore > 1.0f) m->patternScore = 1.0f;

    /* If outputs have been nearly identical for 50+ frames, it's overfitting */
    return m->sameOutputCount > 50;
}

/* ── Environmental Manipulation ──────────────────────────────────── */
static void try_manipulate_environment(Monster *m, Map *map, const Player *p, float dt) {
    m->envTimer -= dt;
    if (m->envTimer > 0) return;

    /* Turn off lights in rooms near the player's path */
    if (m->state == MSTATE_STALK || m->state == MSTATE_INVESTIGATE) {
        /* Find cells near the player and dim them */
        int px = (int)p->posX;
        int py = (int)p->posY;

        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                int cx = px + dx;
                int cy = py + dy;
                if (cx >= 0 && cx < map->width && cy >= 0 && cy < map->height) {
                    if (!(map->cells[cy][cx].flags & CELL_SOLID) &&
                        map->cells[cy][cx].light > 0.2f) {
                        /* Dim the light gradually */
                        map->cells[cy][cx].light *= 0.9f;
                    }
                }
            }
        }
        m->lightsKilled++;
        m->envTimer = 8.0f; /* Cooldown between manipulations */
    }
}

/* ── Main Update ─────────────────────────────────────────────────── */
void monster_update(Monster *m, Map *map, const Player *p, float dt) {
    float distToPlayer = sqrtf((p->posX - m->x)*(p->posX - m->x) + (p->posY - m->y)*(p->posY - m->y));
    bool hasLos = check_los(map, m->x, m->y, p->posX, p->posY);

    /* ── Senses ─────────────────────────────────────────────────── */
    bool heardPlayer = (distToPlayer <= p->noiseRadius);

    /* ── NN Forward Pass ────────────────────────────────────────── */
    float inputs[NN_INPUTS] = {0};
    inputs[0] = fminf(distToPlayer / 20.0f, 1.0f);
    inputs[1] = hasLos ? 1.0f : 0.0f;
    inputs[2] = heardPlayer ? 1.0f : 0.0f;
    inputs[3] = p->panicInput;
    inputs[4] = map_get_light(map, m->x, m->y);
    inputs[5] = fminf(p->timeHiding / 60.0f, 1.0f);
    inputs[6] = fminf(p->timeSprinting / 60.0f, 1.0f);
    inputs[7] = m->menace.tension;
    inputs[8] = m->patternScore;
    inputs[9] = m->menace.isFakingMercy ? 1.0f : 0.0f;
    inputs[10] = (float)m->lightsKilled / 10.0f;

    float outputs[NN_OUTPUTS] = {0};
    nn_forward(&m->brain, inputs, outputs);

    float aggression = outputs[0];
    float chaseThresh = outputs[1];
    float stalkBias = outputs[2];
    float envBias = outputs[3];

    /* ── Overfitting check ──────────────────────────────────────── */
    bool overfitting = check_overfitting(m, outputs);
    if (overfitting && m->state != MSTATE_GLITCH) {
        m->state = MSTATE_GLITCH;
        m->glitchTimer = 5.0f + (float)(rand() % 50) / 10.0f;
    }

    /* ── Menace Gauge ───────────────────────────────────────────── */
    menace_update(&m->menace, distToPlayer, hasLos, dt);

    /* ── Environmental manipulation ─────────────────────────────── */
    if (envBias > 0.5f) {
        try_manipulate_environment(m, map, p, dt);
    }

    /* ── State Machine ──────────────────────────────────────────── */
    if (m->state == MSTATE_GLITCH) {
        /* Glitch state: erratic movement, random direction changes */
        m->glitchTimer -= dt;
        if (m->glitchTimer <= 0) {
            m->state = MSTATE_WANDER;
            m->sameOutputCount = 0;
            m->patternScore = 0;
            /* Mutate some weights to break out of the pattern */
            for (int i = 0; i < 10; i++) {
                int li = rand() % NN_HIDDEN1;
                int lj = rand() % NN_HIDDEN2;
                m->brain.w2[li][lj] += ((float)rand() / RAND_MAX - 0.5f) * 0.5f;
            }
        }

        /* Erratic movement during glitch */
        if ((rand() % 10) < 3) {
            float angle = (float)(rand() % 628) / 100.0f;
            m->dirX = cosf(angle);
            m->dirY = sinf(angle);
        }
        float newX = m->x + m->dirX * m->moveSpeed * 2.5f * dt;
        float newY = m->y + m->dirY * m->moveSpeed * 2.5f * dt;
        if (map_is_walkable(map, newX, m->y)) m->x = newX;
        if (map_is_walkable(map, m->x, newY)) m->y = newY;
        return;
    }

    if (hasLos) {
        m->suspicion += dt * (1.0f + aggression);
        m->lastSeenPlayerX = p->posX;
        m->lastSeenPlayerY = p->posY;

        if (m->menace.isFakingMercy) {
            /* Stalk instead of chase — build dread */
            m->state = MSTATE_STALK;
        } else if (m->suspicion > 0.5f || aggression > chaseThresh) {
            /* Should we stalk first or go straight to chase? */
            if (stalkBias > 0.6f && m->menace.tension < 0.7f && distToPlayer > 3.0f) {
                m->state = MSTATE_STALK;
            } else {
                if (distToPlayer < 2.0f) {
                    /* Drama pause: freeze for a beat before the kill */
                    if (m->menace.dramaPause <= 0 && m->menace.tension > 0.5f) {
                        m->menace.dramaPause = 0.4f;
                    }
                }
                m->state = MSTATE_CHASE;
            }
        }
    } else {
        m->suspicion -= dt * 0.2f;
        if (m->suspicion < 0.0f) m->suspicion = 0.0f;

        if (m->state == MSTATE_CHASE || m->state == MSTATE_STALK) {
            m->state = MSTATE_INVESTIGATE;
            path_find(map, (int)m->x, (int)m->y,
                      (int)m->lastSeenPlayerX, (int)m->lastSeenPlayerY, &m->currentPath);
        }
    }

    if (heardPlayer && m->state != MSTATE_CHASE && m->state != MSTATE_STALK) {
        m->state = MSTATE_INVESTIGATE;
        m->lastSeenPlayerX = p->posX;
        m->lastSeenPlayerY = p->posY;
        path_find(map, (int)m->x, (int)m->y,
                  (int)m->lastSeenPlayerX, (int)m->lastSeenPlayerY, &m->currentPath);
    }

    /* ── Movement ───────────────────────────────────────────────── */
    float currentSpeed = m->moveSpeed * (0.8f + aggression * 0.5f);
    if (m->state == MSTATE_CHASE) currentSpeed *= 1.5f;
    if (m->state == MSTATE_STALK) currentSpeed *= 0.5f; /* Slow stalking */

    /* Drama pause: monster freezes before lunging */
    if (m->menace.dramaPause > 0) {
        return; /* Don't move — dramatic freeze */
    }

    float targetX = m->x;
    float targetY = m->y;

    if (m->state == MSTATE_CHASE) {
        targetX = p->posX;
        targetY = p->posY;
    } else if (m->state == MSTATE_STALK) {
        /* Follow at a distance — stay 4-6 cells behind */
        float dx = p->posX - m->x;
        float dy = p->posY - m->y;
        float d = sqrtf(dx*dx + dy*dy);
        if (d > 6.0f) {
            /* Too far, close in */
            targetX = m->x + (dx / d) * 0.5f;
            targetY = m->y + (dy / d) * 0.5f;
        } else if (d < 3.0f) {
            /* Too close, back off slightly */
            targetX = m->x - (dx / d) * 0.3f;
            targetY = m->y - (dy / d) * 0.3f;
        }
        /* Otherwise hold position */
    } else if (m->state == MSTATE_INVESTIGATE || m->state == MSTATE_WANDER) {
        if (m->currentPath.length > 0 && m->currentPath.current_index < m->currentPath.length) {
            targetX = m->currentPath.points[m->currentPath.current_index].x + 0.5f;
            targetY = m->currentPath.points[m->currentPath.current_index].y + 0.5f;

            float distToNode = sqrtf((targetX - m->x)*(targetX - m->x) + (targetY - m->y)*(targetY - m->y));
            if (distToNode < 0.2f) {
                m->currentPath.current_index++;
            }
        } else {
            if (m->state == MSTATE_INVESTIGATE) {
                m->state = MSTATE_WANDER;
            }

            if (m->state == MSTATE_WANDER && (rand() % 100) < 2) {
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

        if (map_is_walkable(map, newX, m->y)) m->x = newX;
        if (map_is_walkable(map, m->x, newY)) m->y = newY;
    }
}
