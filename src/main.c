/*
 * ╔═══════════════════════════════════════════════════════════════════╗
 * ║                         E R E B U S                             ║
 * ║                  "It doesn't hunt. It studies."                  ║
 * ║                                                                  ║
 * ║  A 2.5D raycasted survival horror where the monster is a real    ║
 * ║  neural network trained on your behavior.                        ║
 * ╚═══════════════════════════════════════════════════════════════════╝
 *
 * Sprint 1: Core raycasting engine with procedural textures,
 *           player movement, and atmospheric rendering.
 */
#include "raylib.h"
#include "engine/raycaster.h"
#include "engine/texgen.h"
#include "engine/postfx.h"
#include "engine/audio.h"
#include "world/map.h"
#include "world/mapgen.h"
#include "game/player.h"
#include "game/items.h"
#include "game/deathcard.h"
#include "game/options.h"
#include "game/panicroom.h"
#include "game/narrative.h"
#include "ai/monster.h"
#include "ai/training.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Game States ──────────────────────────────────────────────────── */
typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_OPTIONS,
    STATE_DEATH,
    STATE_PANIC_ROOM,
    STATE_NARRATIVE_ENDING
} GameState;

/* ── Global Game Context ──────────────────────────────────────────── */
typedef struct {
    GameState       state;
    RaycastRenderer renderer;
    PostFX          postfx;
    Map             map;
    Player          player;
    Monster         monster;

    /* Items & Inventory */
    Item      items[MAX_ITEMS];
    int       itemCount;
    Inventory inventory;

    /* Persistence */
    PlayerProfile profile;
    DeathReason   lastDeathReason;
    float         deathScreenTimer;

    /* Audio */
    AudioEngine   audio;

    /* Training */
    ReplayBuffer  replayBuf;
    float         replayTimer; /* Record samples every N seconds */

    /* Options */
    GameOptions   options;

    /* Panic Room */
    PanicRoom     panicRoom;
    MapGenResult  mapGenResult;
    int           totalSynapsesCut;

    /* Narrative */
    NarrativeState narrative;

    /* Debug */
    bool  showMinimap;
    bool  showDebugInfo;

    /* Timing */
    float runTimer;
    int   runNumber;
} Game;

/* ── Draw the debug minimap ───────────────────────────────────────── */
static void draw_minimap(const Game *g) {
    int mapSize = 6;  /* pixels per cell */
    int ox = 10, oy = 10;

    /* Background */
    DrawRectangle(ox - 2, oy - 2,
                  g->map.width * mapSize + 4,
                  g->map.height * mapSize + 4,
                  (Color){0, 0, 0, 180});

    for (int y = 0; y < g->map.height; y++) {
        for (int x = 0; x < g->map.width; x++) {
            Color c;
            if (g->map.cells[y][x].flags & CELL_SOLID) {
                c = (Color){60, 60, 70, 255};
            } else if (g->map.cells[y][x].flags & CELL_OBJECTIVE) {
                c = (Color){0, 255, 200, 255};
            } else if (g->map.cells[y][x].flags & CELL_EXIT) {
                c = (Color){255, 200, 0, 255};
            } else {
                /* Shade by light level */
                int v = (int)(g->map.cells[y][x].light * 40);
                c = (Color){v, v, v + 5, 255};
            }
            DrawRectangle(ox + x * mapSize, oy + y * mapSize, mapSize - 1, mapSize - 1, c);
        }
    }

    /* Player position and direction */
    int px = ox + (int)(g->player.posX * mapSize);
    int py = oy + (int)(g->player.posY * mapSize);
    DrawCircle(px, py, 3, (Color){0, 255, 100, 255});

    /* Direction line */
    int dx = px + (int)(g->player.dirX * 12);
    int dy = py + (int)(g->player.dirY * 12);
    DrawLine(px, py, dx, dy, (Color){0, 255, 100, 200});
}

/* ── Draw the HUD ─────────────────────────────────────────────────── */
static void draw_hud(const Game *g) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* Crosshair — subtle dot */
    DrawCircle(sw / 2, sh / 2, 2, (Color){0, 255, 200, 80});

    /* Memory Cores (top left) */
    DrawText(TextFormat("CORES: %d / %d", g->inventory.memoryCores, g->inventory.totalCoresNeeded),
             20, 12, 14, (Color){0, 255, 200, 200});

    /* Movement mode indicator (bottom left) */
    const char *modeStr = "WALK";
    Color modeColor = (Color){150, 150, 160, 200};
    switch (g->player.moveMode) {
        case MOVE_SPRINT: modeStr = "SPRINT"; modeColor = (Color){255, 100, 80, 200};  break;
        case MOVE_WALK:   modeStr = "WALK";   modeColor = (Color){150, 150, 160, 200}; break;
        case MOVE_CROUCH: modeStr = "CROUCH"; modeColor = (Color){100, 200, 180, 200}; break;
        case MOVE_FREEZE: modeStr = "FREEZE"; modeColor = (Color){80, 80, 220, 200};   break;
    }
    DrawText(modeStr, 20, sh - 30, 14, modeColor);

    /* Flashlight battery (bottom right) */
    if (g->player.flashlightOn) {
        int barW = 60, barH = 6;
        int bx = sw - barW - 20, by = sh - 25;
        DrawRectangle(bx - 1, by - 1, barW + 2, barH + 2, (Color){40, 40, 40, 200});
        int fillW = (int)(barW * g->player.flashlightBattery);
        Color batColor = g->player.flashlightBattery > 0.3f ?
                         (Color){0, 200, 160, 200} : (Color){255, 60, 60, 200};
        DrawRectangle(bx, by, fillW, barH, batColor);
        DrawText("LIGHT", bx, by - 14, 10, (Color){120, 120, 130, 200});
    } else {
        DrawText("[F] LIGHT OFF", sw - 120, sh - 30, 10, (Color){80, 80, 80, 150});
    }

    /* Run number (bottom center) */
    const char *runStr = TextFormat("RUN #%d", g->runNumber);
    int runW = MeasureText(runStr, 10);
    DrawText(runStr, (sw - runW) / 2, sh - 16, 10, (Color){60, 60, 70, 150});
}

/* ── Draw debug info ──────────────────────────────────────────────── */
static void draw_debug(const Game *g) {
    int y = GetScreenHeight() - 100;
    Color c = (Color){0, 255, 200, 200};
    DrawText(TextFormat("FPS: %d", GetFPS()), 10, y, 14, c);
    DrawText(TextFormat("POS: %.1f, %.1f", g->player.posX, g->player.posY), 10, y + 16, 14, c);
    DrawText(TextFormat("ANG: %.2f", g->player.angle), 10, y + 32, 14, c);
    DrawText(TextFormat("NOISE: %.1f", g->player.noiseRadius), 10, y + 48, 14, c);
    DrawText(TextFormat("PANIC: %.2f", g->player.panicInput), 10, y + 64, 14, c);
    DrawText(TextFormat("M-STATE: %d", g->monster.state), 10, y + 80, 14, c);
}

/* ── Title Screen ─────────────────────────────────────────────────── */
static void draw_title(float time) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    ClearBackground(BLACK);

    /* Title */
    const char *title = "E R E B U S";
    int tw = MeasureText(title, 60);
    Color titleCol = (Color){
        (unsigned char)(200 + (int)(sinf(time * 0.5f) * 55)),
        (unsigned char)(255),
        (unsigned char)(210 + (int)(sinf(time * 0.7f) * 45)),
        255
    };
    DrawText(title, (sw - tw) / 2, sh / 2 - 60, 60, titleCol);

    /* Subtitle */
    const char *sub = "It doesn't hunt. It studies.";
    int subw = MeasureText(sub, 16);
    DrawText(sub, (sw - subw) / 2, sh / 2 + 10, 16, (Color){100, 100, 120, 200});

    /* Prompt */
    if ((int)(time * 2) % 2 == 0) {
        const char *prompt = "[ PRESS ENTER TO BEGIN ]";
        int pw = MeasureText(prompt, 14);
        DrawText(prompt, (sw - pw) / 2, sh / 2 + 80, 14, (Color){0, 200, 160, 180});
    }

    /* Version */
    DrawText("v0.3 // THE LEARNING DARK", 10, sh - 20, 10, (Color){40, 40, 50, 150});
}

/* ═════════════════════════════════════════════════════════════════════
 *                           M A I N
 * ═════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* ── Window Setup ──────────────────────────────────────────── */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "EREBUS — It doesn't hunt. It studies.");
    SetTargetFPS(60);

    /* Lock cursor for FPS controls */
    DisableCursor();

    /* ── Initialize Game ───────────────────────────────────────── */
    Game game = {0};
    game.state       = STATE_TITLE;
    game.showMinimap = true;
    game.showDebugInfo = false;
    game.runNumber   = 0;
    game.runTimer    = 13.0f * 60.0f; /* 13 minutes */

    /* Initialize renderer */
    rc_init(&game.renderer);

    /* Generate procedural textures (seed = 42 for reproducibility) */
    texgen_generate_all(&game.renderer, 42);

    /* Initialize post-processing */
    postfx_init(&game.postfx);

    /* Load test map */
    map_init(&game.map);
    map_load_test(&game.map);

    /* Initialize player at spawn point */
    player_init(&game.player, game.map.spawnX, game.map.spawnY, game.map.spawnAngle);

    /* Initialize monster at spawn point */
    monster_init(&game.monster, &game.renderer, game.map.monsterX, game.map.monsterY);

    /* Initialize items from map */
    items_init(game.items, &game.itemCount, &game.map, &game.renderer);
    inventory_init(&game.inventory, game.map.objectiveCount);

    /* Initialize player profile (load if exists) */
    profile_init(&game.profile);
    profile_load(&game.profile, "erebus_profile.dat");

    /* Initialize audio engine */
    audio_init(&game.audio);

    /* Initialize options (load if exists) */
    options_init(&game.options);
    options_load(&game.options, "erebus_options.dat");

    /* Initialize replay buffer for training */
    replay_init(&game.replayBuf);
    game.replayTimer = 0;

    /* Initialize narrative */
    narrative_init(&game.narrative);
    game.totalSynapsesCut = 0;

    /* Ensure nearest-neighbor filtering for crisp pixels */
    SetTextureFilter(game.renderer.screenTex, TEXTURE_FILTER_POINT);

    float titleTime = 0;

    /* ── Main Loop ─────────────────────────────────────────────── */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f; /* Cap delta time */

        switch (game.state) {

        /* ── TITLE SCREEN ──────────────────────────────────────── */
        case STATE_TITLE:
            titleTime += dt;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                game.state = STATE_PLAYING;
                game.runNumber++;
            }

            BeginDrawing();
            draw_title(titleTime);
            EndDrawing();
            break;

        /* ── PLAYING ───────────────────────────────────────────── */
        case STATE_PLAYING:
            /* Update player */
            player_update(&game.player, &game.map, dt);

            /* Update monster */
            monster_update(&game.monster, &game.map, &game.player, dt);
            
            /* Update monster sprite position for renderer */
            if (game.monster.spriteId >= 0 && game.monster.spriteId < RC_MAX_SPRITES) {
                game.renderer.sprites[game.monster.spriteId].x = game.monster.x;
                game.renderer.sprites[game.monster.spriteId].y = game.monster.y;
            }

            /* Update items (collection) */
            items_update(game.items, game.itemCount, &game.player, &game.inventory, dt);

            /* Update audio engine */
            {
                float mdx2 = game.monster.x - game.player.posX;
                float mdy2 = game.monster.y - game.player.posY;
                float mDist = sqrtf(mdx2*mdx2 + mdy2*mdy2);
                audio_update(&game.audio, &game.player, mDist, dt);
            }

            /* Record replay samples for training (every 0.5s) */
            game.replayTimer += dt;
            if (game.replayTimer >= 0.5f) {
                game.replayTimer = 0;
                float mdx2 = game.monster.x - game.player.posX;
                float mdy2 = game.monster.y - game.player.posY;
                float mDist = sqrtf(mdx2*mdx2 + mdy2*mdy2);
                float reward = (mDist < 3.0f) ? 1.0f : (mDist < 8.0f) ? 0.3f : -0.1f;
                replay_record(&game.replayBuf, game.monster.brain.a0, game.monster.brain.a3, reward);
            }
            /* Check if monster caught player */
            {
                float mdx = game.monster.x - game.player.posX;
                float mdy = game.monster.y - game.player.posY;
                float mdist = sqrtf(mdx*mdx + mdy*mdy);
                if (mdist < 0.5f) {
                    game.lastDeathReason = DEATH_CAUGHT;
                    game.deathScreenTimer = 0;
                    game.state = STATE_DEATH;
                    EnableCursor();
                }
            }

            /* Check if player reached exit with all cores */
            {
                int px = (int)game.player.posX;
                int py = (int)game.player.posY;
                if (px >= 0 && px < game.map.width && py >= 0 && py < game.map.height) {
                    if ((game.map.cells[py][px].flags & CELL_EXIT) &&
                        game.inventory.memoryCores >= game.inventory.totalCoresNeeded) {
                        game.lastDeathReason = DEATH_SURVIVED;
                        game.deathScreenTimer = 0;
                        game.state = STATE_DEATH;
                        EnableCursor();
                    }
                }
            }

            /* Update run timer */
            game.runTimer -= dt;
            if (game.runTimer <= 0) {
                game.runTimer = 0;
                game.lastDeathReason = DEATH_TIMER;
                game.deathScreenTimer = 0;
                game.state = STATE_DEATH;
                EnableCursor();
            }

            /* Toggle debug keys */
            if (IsKeyPressed(KEY_TAB)) game.showMinimap = !game.showMinimap;
            if (IsKeyPressed(KEY_F3))  game.showDebugInfo = !game.showDebugInfo;

            /* Pause */
            if (IsKeyPressed(KEY_ESCAPE)) {
                game.state = STATE_PAUSED;
                EnableCursor();
            }

            /* ── Render ────────────────────────────────────────── */
            /* Cast rays and fill the pixel buffer */
            rc_render(&game.renderer, &game.map, &game.player);

            /* Drive post-FX from monster state */
            {
                float mdx3 = game.monster.x - game.player.posX;
                float mdy3 = game.monster.y - game.player.posY;
                float mDist3 = sqrtf(mdx3*mdx3 + mdy3*mdy3);

                /* Chromatic aberration increases when monster is close */
                float targetCA = (mDist3 < 8.0f) ? (8.0f - mDist3) / 8.0f : 0.0f;
                game.postfx.chromaticAberration += (targetCA - game.postfx.chromaticAberration) * 0.05f;

                /* Danger tint pulses red when monster is near */
                float targetDanger = (mDist3 < 6.0f) ? (6.0f - mDist3) / 6.0f : 0.0f;
                game.postfx.dangerTint += (targetDanger - game.postfx.dangerTint) * 0.08f;

                /* Glitch effect when monster is in GLITCH state */
                float targetGlitch = (game.monster.state == MSTATE_GLITCH) ? 0.8f : 0.0f;
                game.postfx.glitchAmount += (targetGlitch - game.postfx.glitchAmount) * 0.1f;

                /* More noise when monster is stalking */
                if (game.monster.state == MSTATE_STALK && mDist3 < 8.0f) {
                    game.postfx.noiseAmount = 0.04f + (8.0f - mDist3) * 0.01f;
                } else {
                    game.postfx.noiseAmount = 0.02f;
                }
            }

            /* Apply post-processing to the buffer */
            postfx_apply(&game.postfx, game.renderer.buffer, RC_WIDTH, RC_HEIGHT);

            /* Draw everything */
            BeginDrawing();
            ClearBackground(BLACK);

            /* Present the raycasted view scaled to window */
            rc_present(&game.renderer);

            /* HUD overlay (drawn at screen resolution) */
            draw_hud(&game);

            /* Timer display (top center) */
            {
                int mins = (int)game.runTimer / 60;
                int secs = (int)game.runTimer % 60;
                const char *timeStr = TextFormat("%02d:%02d", mins, secs);
                int tw = MeasureText(timeStr, 18);
                Color timeCol = (game.runTimer < 120) ?
                    (Color){255, 50, 50, 220} : (Color){200, 200, 210, 180};
                DrawText(timeStr, (GetScreenWidth() - tw) / 2, 12, 18, timeCol);
            }

            /* Minimap */
            if (game.showMinimap) draw_minimap(&game);

            /* Debug info */
            if (game.showDebugInfo) draw_debug(&game);

            /* Architect mode: subtle messages from the monster */
            narrative_draw_architect_hints(&game.narrative, dt);

            EndDrawing();
            break;

        /* ── PAUSED ────────────────────────────────────────────── */
        case STATE_PAUSED: {
            if (IsKeyPressed(KEY_ESCAPE)) {
                game.state = STATE_PLAYING;
                DisableCursor();
            }
            if (IsKeyPressed(KEY_O)) {
                game.state = STATE_OPTIONS;
            }

            BeginDrawing();
            ClearBackground((Color){5, 5, 10, 255});

            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            const char *pauseText = "P A U S E D";
            int pw = MeasureText(pauseText, 40);
            DrawText(pauseText, (sw - pw) / 2, sh / 2 - 50, 40, (Color){0, 255, 200, 200});

            const char *resume = "[ ESC ] Resume";
            int rw = MeasureText(resume, 16);
            DrawText(resume, (sw - rw) / 2, sh / 2 + 10, 16, (Color){100, 100, 120, 180});

            const char *optStr = "[ O ] Options";
            int ow = MeasureText(optStr, 16);
            DrawText(optStr, (sw - ow) / 2, sh / 2 + 35, 16, (Color){100, 100, 120, 180});

            DrawText(TextFormat("Run #%d", game.runNumber), 20, sh - 30, 12, (Color){60, 60, 70, 150});

            EndDrawing();
            break;
        }

        /* ── OPTIONS ───────────────────────────────────────────── */
        case STATE_OPTIONS: {
            BeginDrawing();
            bool closed = options_draw(&game.options);
            EndDrawing();

            if (closed) {
                /* Apply audio settings */
                audio_set_volumes(&game.audio, game.options.masterVolume,
                                  game.options.sfxVolume, game.options.musicVolume);
                /* Apply post-fx settings */
                game.postfx.enabled = game.options.postFXEnabled;
                game.postfx.scanlineIntensity = game.options.scanlineIntensity;
                game.postfx.vignetteStrength = game.options.vignetteStrength;
                game.showMinimap = game.options.showMinimap;
                /* Save options */
                options_save(&game.options, "erebus_options.dat");
                /* Fullscreen toggle */
                if (game.options.fullscreen && !IsWindowFullscreen()) ToggleFullscreen();
                if (!game.options.fullscreen && IsWindowFullscreen()) ToggleFullscreen();
                game.state = STATE_PAUSED;
            }
            break;
        }

        /* ── DEATH SCREEN ──────────────────────────────────────── */
        case STATE_DEATH: {
            game.deathScreenTimer += dt;

            /* Record this run */
            if (game.deathScreenTimer < dt * 2) { /* Only on first frame */
                RunRecord rec = {0};
                rec.runNumber = game.runNumber;
                rec.reason = game.lastDeathReason;
                rec.survivalTime = 13.0f * 60.0f - game.runTimer;
                rec.timeSprinting = game.player.timeSprinting;
                rec.timeHiding = game.player.timeHiding;
                rec.timeMoving = game.player.timeMoving;
                rec.avgPanic = game.player.panicInput;
                rec.coresCollected = game.inventory.memoryCores;
                rec.totalCores = game.inventory.totalCoresNeeded;
                profile_record_run(&game.profile, &rec);
                profile_save(&game.profile, "erebus_profile.dat");
                nn_save(&game.monster.brain, "erebus_brain.dat");

                /* TRAIN the neural network on this run's data */
                training_run(&game.monster.brain, &game.replayBuf, &game.profile);
                training_mutate(&game.monster.brain, &game.profile, game.runNumber * 7919);
                replay_init(&game.replayBuf); /* Clear buffer for next run */

                audio_play_death(&game.audio);
            }

            if (IsKeyPressed(KEY_ENTER)) {
                bool escaped = (game.lastDeathReason == DEATH_SURVIVED);
                EndingType ending = narrative_check_ending(&game.narrative, &game.profile, game.totalSynapsesCut, escaped);
                
                if (ending != ENDING_NONE && ending != ENDING_LOBOTOMY) {
                    game.state = STATE_NARRATIVE_ENDING;
                } else {
                    /* Go to Panic Room between runs */
                    int surgPts = 1 + (int)(game.player.timeHiding / 30.0f);
                    panicroom_init(&game.panicRoom, surgPts);
                    game.state = STATE_PANIC_ROOM;
                }
            }

            BeginDrawing();
            ClearBackground((Color){5, 2, 2, 255});

            int sw = GetScreenWidth();
            int sh = GetScreenHeight();

            /* Death reason */
            const char *deathMsg;
            Color deathCol;
            switch (game.lastDeathReason) {
                case DEATH_CAUGHT:
                    deathMsg = "IT FOUND YOU";
                    deathCol = (Color){255, 30, 30, 255};
                    break;
                case DEATH_TIMER:
                    deathMsg = "TIME EXPIRED";
                    deathCol = (Color){200, 100, 50, 255};
                    break;
                case DEATH_SURVIVED:
                    deathMsg = "Y O U  E S C A P E D";
                    deathCol = (Color){0, 255, 200, 255};
                    break;
                default:
                    deathMsg = "UNKNOWN";
                    deathCol = WHITE;
                    break;
            }

            int dmw = MeasureText(deathMsg, 40);
            DrawText(deathMsg, (sw - dmw) / 2, sh / 2 - 80, 40, deathCol);

            /* Stats */
            float survived = 13.0f * 60.0f - game.runTimer;
            int smins = (int)survived / 60;
            int ssecs = (int)survived % 60;

            Color statCol = (Color){120, 120, 130, 200};
            DrawText(TextFormat("Survived: %02d:%02d", smins, ssecs), sw/2 - 80, sh/2 - 20, 14, statCol);
            DrawText(TextFormat("Cores: %d / %d", game.inventory.memoryCores, game.inventory.totalCoresNeeded), sw/2 - 80, sh/2, 14, statCol);
            DrawText(TextFormat("Sprint Time: %.0fs", game.player.timeSprinting), sw/2 - 80, sh/2 + 20, 14, statCol);
            DrawText(TextFormat("Hide Time: %.0fs", game.player.timeHiding), sw/2 - 80, sh/2 + 40, 14, statCol);

            /* Neural network testimony */
            DrawText("// NEURAL NETWORK TESTIMONY //", sw/2 - 120, sh/2 + 80, 12, (Color){255, 50, 50, 150});
            DrawText(TextFormat("Weights updated. Run %d recorded.", game.runNumber), sw/2 - 110, sh/2 + 100, 12, (Color){200, 60, 60, 120});
            DrawText("It remembers.", sw/2 - 50, sh/2 + 120, 14, (Color){255, 0, 0, (unsigned char)(100 + (int)(sinf(game.deathScreenTimer * 2) * 80))});

            /* Continue prompt */
            if (game.deathScreenTimer > 2.0f && ((int)(game.deathScreenTimer * 2) % 2 == 0)) {
                const char *contMsg = "[ ENTER ] Begin Next Run";
                int cmw = MeasureText(contMsg, 14);
                DrawText(contMsg, (sw - cmw) / 2, sh - 50, 14, (Color){0, 200, 160, 180});
            }

            EndDrawing();
            break;
        }

        /* -- PANIC ROOM ------------------------------------------ */
        case STATE_PANIC_ROOM: {
            BeginDrawing();
            bool done = panicroom_update_and_draw(&game.panicRoom, &game.monster.brain,
                                                   &game.profile, dt);
            EndDrawing();

            if (done) {
                game.totalSynapsesCut += game.panicRoom.synapsesCutThisSession;
                EndingType ending = narrative_check_ending(&game.narrative, &game.profile, game.totalSynapsesCut, false);
                
                if (ending == ENDING_LOBOTOMY) {
                    game.state = STATE_NARRATIVE_ENDING;
                } else {
                    /* Generate new procedural map for next run */
                    game.runNumber++;
                    game.runTimer = 13.0f * 60.0f;

                /* Reset renderer sprites for new map */
                game.renderer.spriteCount = 0;

                /* Procedural map (seed from run number for variety) */
                mapgen_generate(&game.map, (unsigned int)(game.runNumber * 31337),
                                24, 24, &game.mapGenResult);

                /* Reinitialize player, monster, items */
                player_init(&game.player, game.map.spawnX, game.map.spawnY, game.map.spawnAngle);
                monster_init(&game.monster, &game.renderer, game.map.monsterX, game.map.monsterY);
                items_init(game.items, &game.itemCount, &game.map, &game.renderer);
                inventory_init(&game.inventory, game.map.objectiveCount);

                nn_save(&game.monster.brain, "erebus_brain.dat");

                game.state = STATE_PLAYING;
                DisableCursor();
            }
            break;
        }

        /* -- NARRATIVE ENDING ------------------------------------ */
        case STATE_NARRATIVE_ENDING: {
            BeginDrawing();
            bool done = narrative_draw_ending(&game.narrative, game.narrative.lastEnding, dt);
            EndDrawing();

            if (done) {
                /* Return to title screen after an ending */
                game.state = STATE_TITLE;
                game.runNumber = 1;
                game.totalSynapsesCut = 0;
            }
            break;
        }

        default:
            break;
        }
    }

    /* ── Cleanup ────────────────────────────────────────────────── */
    profile_save(&game.profile, "erebus_profile.dat");
    nn_save(&game.monster.brain, "erebus_brain.dat");
    options_save(&game.options, "erebus_options.dat");
    audio_free(&game.audio);
    rc_free(&game.renderer);
    CloseWindow();
    return 0;
}
