/*
 * EREBUS — Options Menu Implementation
 */
#include "game/options.h"
#include <stdio.h>
#include <string.h>

void options_init(GameOptions *opts) {
    opts->masterVolume = 0.7f;
    opts->sfxVolume = 0.8f;
    opts->musicVolume = 0.5f;
    opts->fullscreen = false;
    opts->showFPS = false;
    opts->showMinimap = true;
    opts->mouseSensitivity = 1.0f;
    opts->postFXEnabled = true;
    opts->scanlineIntensity = 0.15f;
    opts->vignetteStrength = 0.4f;
}

bool options_save(const GameOptions *opts, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    fwrite(opts, sizeof(GameOptions), 1, f);
    fclose(f);
    return true;
}

bool options_load(GameOptions *opts, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    size_t r = fread(opts, sizeof(GameOptions), 1, f);
    fclose(f);
    return r == 1;
}

/* Simple slider helper */
static float draw_slider(const char *label, float value, float minV, float maxV,
                          int x, int y, int w) {
    Color labelCol = (Color){180, 180, 190, 220};
    Color barBg = (Color){30, 30, 35, 200};
    Color barFg = (Color){0, 200, 160, 220};

    DrawText(label, x, y, 14, labelCol);

    int barX = x + 180;
    int barY = y + 2;
    int barH = 10;

    DrawRectangle(barX, barY, w, barH, barBg);
    float norm = (value - minV) / (maxV - minV);
    DrawRectangle(barX, barY, (int)(w * norm), barH, barFg);

    /* Handle mouse interaction */
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
        mouse.x >= barX && mouse.x <= barX + w &&
        mouse.y >= barY - 5 && mouse.y <= barY + barH + 5) {
        norm = (mouse.x - barX) / (float)w;
        if (norm < 0) norm = 0;
        if (norm > 1) norm = 1;
        value = minV + norm * (maxV - minV);
    }

    /* Display value */
    DrawText(TextFormat("%.0f%%", norm * 100), barX + w + 10, y, 12, labelCol);

    return value;
}

static bool draw_toggle(const char *label, bool value, int x, int y) {
    Color labelCol = (Color){180, 180, 190, 220};
    DrawText(label, x, y, 14, labelCol);

    int bx = x + 180;
    Color boxCol = value ? (Color){0, 200, 160, 220} : (Color){60, 60, 70, 200};
    DrawRectangle(bx, y, 30, 14, boxCol);
    DrawText(value ? "ON" : "OFF", bx + 4, y, 12, WHITE);

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        mouse.x >= bx && mouse.x <= bx + 30 &&
        mouse.y >= y && mouse.y <= y + 14) {
        value = !value;
    }

    return value;
}

bool options_draw(GameOptions *opts) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* Background */
    DrawRectangle(0, 0, sw, sh, (Color){5, 5, 10, 240});

    /* Title */
    const char *title = "O P T I O N S";
    int tw = MeasureText(title, 30);
    DrawText(title, (sw - tw) / 2, 40, 30, (Color){0, 255, 200, 220});

    int x = sw / 2 - 160;
    int y = 100;
    int sliderW = 150;

    /* Audio */
    DrawText("-- AUDIO --", x, y, 12, (Color){100, 100, 120, 180});
    y += 25;
    opts->masterVolume = draw_slider("Master Volume", opts->masterVolume, 0, 1, x, y, sliderW);
    y += 25;
    opts->sfxVolume = draw_slider("SFX Volume", opts->sfxVolume, 0, 1, x, y, sliderW);
    y += 25;
    opts->musicVolume = draw_slider("Music Volume", opts->musicVolume, 0, 1, x, y, sliderW);
    y += 35;

    /* Display */
    DrawText("-- DISPLAY --", x, y, 12, (Color){100, 100, 120, 180});
    y += 25;
    opts->mouseSensitivity = draw_slider("Mouse Sensitivity", opts->mouseSensitivity, 0.1f, 3.0f, x, y, sliderW);
    y += 25;
    opts->fullscreen = draw_toggle("Fullscreen", opts->fullscreen, x, y);
    y += 25;
    opts->showFPS = draw_toggle("Show FPS", opts->showFPS, x, y);
    y += 25;
    opts->showMinimap = draw_toggle("Show Minimap", opts->showMinimap, x, y);
    y += 35;

    /* Post-FX */
    DrawText("-- EFFECTS --", x, y, 12, (Color){100, 100, 120, 180});
    y += 25;
    opts->postFXEnabled = draw_toggle("Post-Processing", opts->postFXEnabled, x, y);
    y += 25;
    opts->scanlineIntensity = draw_slider("Scanlines", opts->scanlineIntensity, 0, 0.5f, x, y, sliderW);
    y += 25;
    opts->vignetteStrength = draw_slider("Vignette", opts->vignetteStrength, 0, 1, x, y, sliderW);

    /* Close hint */
    const char *closeMsg = "[ ESC ] Close";
    int cmw = MeasureText(closeMsg, 14);
    DrawText(closeMsg, (sw - cmw) / 2, sh - 40, 14, (Color){0, 200, 160, 180});

    return IsKeyPressed(KEY_ESCAPE);
}
