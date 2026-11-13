#include "apps.h"
#include "../Drivers/screen/screen.h"
#include "../Drivers/screen/notify.h"

#define MAX_SNAKE 128
#define GRID_W    24
#define GRID_H    16

static int is_open = 0;
static int is_max = 0;
static int sx = 280, sy = 110;
static int dragging = 0, dox = 0, doy = 0;

static int body_x[MAX_SNAKE];
static int body_y[MAX_SNAKE];
static int snake_len = 4;
static int dir_x = 1, dir_y = 0;
static int food_x = 14, food_y = 8;
static int game_over = 0;
static int tick_div = 0;
static uint32_t rng_seed = 777;

static void spawn_food(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    food_x = (int)((rng_seed >> 16) % GRID_W);
    rng_seed = rng_seed * 1103515245 + 12345;
    food_y = (int)((rng_seed >> 16) % GRID_H);
}

static void reset_game(void) {
    snake_len = 4; dir_x = 1; dir_y = 0; game_over = 0; tick_div = 0;
    for (int i = 0; i < snake_len; i++) {
        *(body_x + i) = 8 - i;
        *(body_y + i) = 8;
    }
    spawn_food();
}

void gui_snake_open(void) {
    if (!is_open) reset_game();
    is_open = 1;
}

void gui_snake_close(void) {
    is_open = 0;
}

void gui_snake_update(mouse_state_t m, uint8_t click, uint8_t sc) {
    if (!is_open) return;

    int wx = is_max ? 0 : sx;
    int wy = is_max ? 0 : sy;
    int ww = is_max ? g_scr_w : 540;

    if (click) {
        if (m.x >= wx + ww - 44 && m.x <= wx + ww - 8 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_open = 0; return;
        } else if (m.x >= wx + ww - 86 && m.x <= wx + ww - 50 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_max = !is_max; return;
        } else if (!is_max && m.x >= wx && m.x <= wx + ww && m.y >= wy && m.y <= wy + 38) {
            dragging = 1; dox = m.x - sx; doy = m.y - sy;
        }
    }

    if (!m.left_btn) dragging = 0;
    else if (dragging && !is_max) {
        sx = m.x - dox; sy = m.y - doy;
        if (sx < 0) sx = 0;
        if (sx > g_scr_w - 540) sx = g_scr_w - 540;
        if (sy < 0) sy = 0;
        if (sy > g_scr_h - 440) sy = g_scr_h - 440;
    }

    if (sc) {
        if (game_over && (sc == 0x1C || sc == 0x39)) {
            reset_game(); return;
        }
        if ((sc == 0x11 || sc == 0x48) && dir_y != 1)  { dir_x = 0; dir_y = -1; }
        if ((sc == 0x1F || sc == 0x50) && dir_y != -1) { dir_x = 0; dir_y = 1; }
        if ((sc == 0x1E || sc == 0x4B) && dir_x != 1)  { dir_x = -1; dir_y = 0; }
        if ((sc == 0x20 || sc == 0x4D) && dir_x != -1) { dir_x = 1; dir_y = 0; }
    }

    if (game_over) return;

    tick_div++;
    if (tick_div < 7) return;
    tick_div = 0;

    int nx = *(body_x + 0) + dir_x;
    int ny = *(body_y + 0) + dir_y;

    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
        game_over = 1;
        notify_push("Snake", "Game Over! Press Enter");
        return;
    }

    for (int i = 0; i < snake_len; i++) {
        if (*(body_x + i) == nx && *(body_y + i) == ny) {
            game_over = 1;
            notify_push("Snake", "Crash! Press Enter");
            return;
        }
    }

    for (int i = snake_len; i > 0; i--) {
        *(body_x + i) = *(body_x + i - 1);
        *(body_y + i) = *(body_y + i - 1);
    }
    *(body_x + 0) = nx;
    *(body_y + 0) = ny;

    if (nx == food_x && ny == food_y) {
        if (snake_len < MAX_SNAKE - 1) snake_len++;
        spawn_food();
    }
}

void gui_snake_draw(void) {
    if (!is_open) return;

    int wx = is_max ? 0 : sx;
    int wy = is_max ? 0 : sy;
    int ww = is_max ? g_scr_w : 540;
    int wh = is_max ? (g_scr_h - 44) : 400;

    screen_rect_alpha(wx, wy, ww, wh, 0x121820, 225);
    screen_round_rect(wx, wy, ww, 38, 8, 0x1B5E20);
    screen_str(wx + 16, wy + 11, "Sashix Snake", 0xFFFFFF);

    screen_round_rect(wx + ww - 86, wy + 5, 34, 28, 6, 0x2E7D32);
    screen_str(wx + ww - 76, wy + 11, is_max ? "v" : "^", 0xFFFFFF);

    screen_round_rect(wx + ww - 44, wy + 5, 34, 28, 6, 0xC62828);
    screen_str(wx + ww - 34, wy + 11, "X", 0xFFFFFF);

    int cell = is_max ? 36 : 20;
    int gx = wx + (ww - GRID_W * cell) / 2;
    int gy = wy + 44 + ((wh - 52) - GRID_H * cell) / 2;

    screen_rect(gx, gy, GRID_W * cell, GRID_H * cell, 0x0B0F14);
    screen_round_rect(gx + food_x * cell + 2, gy + food_y * cell + 2, cell - 4, cell - 4, 6, 0xFF5252);

    for (int i = 0; i < snake_len; i++) {
        uint32_t col = (i == 0) ? 0x69F0AE : 0x00C853;
        screen_round_rect(gx + *(body_x + i) * cell + 1, gy + *(body_y + i) * cell + 1, cell - 2, cell - 2, 4, col);
    }

    if (game_over) {
        screen_round_rect(wx + ww / 2 - 140, wy + wh / 2 - 24, 280, 48, 8, 0xB71C1C);
        screen_str(wx + ww / 2 - 120, wy + wh / 2 - 8, "GAME OVER (ENTER)", 0xFFFFFF);
    }
}