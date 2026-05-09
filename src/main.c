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
#include "world/map.h"
#include "game/player.h"

#include <stdio.h>
#include <string.h>

/* ── Game States ──────────────────────────────────────────────────── */
typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_DEATH,
    STATE_PANIC_ROOM
} GameState;

/* ── Global Game Context ──────────────────────────────────────────── */
typedef struct {
    GameState       state;
    RaycastRenderer renderer;
    PostFX          postfx;
    Map             map;
    Player          player;

    /* Debug */
    bool  showMinimap;
    bool  showDebugInfo;

    /* Timing */
    float runTimer;       /* 13 minutes countdown */
    int   runNumber;      /* Current run count     */
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
    DrawText("SPRINT 1 // ENGINE FOUNDATION", 10, sh - 20, 10, (Color){40, 40, 50, 150});
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

            /* Update run timer */
            game.runTimer -= dt;
            if (game.runTimer <= 0) {
                game.runTimer = 0;
                /* Time's up — death (to be implemented) */
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

            EndDrawing();
            break;

        /* ── PAUSED ────────────────────────────────────────────── */
        case STATE_PAUSED: {
            if (IsKeyPressed(KEY_ESCAPE)) {
                game.state = STATE_PLAYING;
                DisableCursor();
            }

            BeginDrawing();
            ClearBackground((Color){5, 5, 10, 255});

            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            const char *pauseText = "P A U S E D";
            int pw = MeasureText(pauseText, 40);
            DrawText(pauseText, (sw - pw) / 2, sh / 2 - 30, 40, (Color){0, 255, 200, 200});

            const char *resume = "[ ESC ] Resume";
            int rw = MeasureText(resume, 16);
            DrawText(resume, (sw - rw) / 2, sh / 2 + 30, 16, (Color){100, 100, 120, 180});

            DrawText(TextFormat("Run #%d", game.runNumber), 20, sh - 30, 12, (Color){60, 60, 70, 150});

            EndDrawing();
            break;
        }

        default:
            break;
        }
    }

    /* ── Cleanup ────────────────────────────────────────────────── */
    rc_free(&game.renderer);
    CloseWindow();
    return 0;
}
