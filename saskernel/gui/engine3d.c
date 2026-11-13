#include "apps.h"
#include "../Drivers/screen/screen.h"

#define LUT_SIZE   64
#define VERT_COUNT 8
#define EDGE_COUNT 12

static int is_open = 0;
static int is_max = 0;
static int ex = 300, ey = 120;
static int dragging = 0, dox = 0, doy = 0;

static int ang_x = 8;
static int ang_y = 6;
static int zoom = 110;
static int auto_rot = 1;
static int tick_div = 0;

static const int sin_lut[LUT_SIZE] = {
       0,   25,   50,   74,   98,  120,  142,  162,  180,  197,  212,  226,  237,  246,  252,  255,
     256,  255,  252,  246,  237,  226,  212,  197,  180,  162,  142,  120,   98,   74,   50,   25,
       0,  -25,  -50,  -74,  -98, -120, -142, -162, -180, -197, -212, -226, -237, -246, -252, -255,
    -256, -255, -252, -246, -237, -226, -212, -197, -180, -162, -142, -120,  -98,  -74,  -50,  -25
};

static const int cube_vx[VERT_COUNT] = { -100,  100,  100, -100, -100,  100,  100, -100 };
static const int cube_vy[VERT_COUNT] = { -100, -100,  100,  100, -100, -100,  100,  100 };
static const int cube_vz[VERT_COUNT] = { -100, -100, -100, -100,  100,  100,  100,  100 };

static const int edge_a[EDGE_COUNT]  = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 };
static const int edge_b[EDGE_COUNT]  = { 1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7 };

static void draw_line(int x0, int y0, int x1, int y1, uint32_t col, int min_x, int min_y, int max_x, int max_y) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int steps = 0;
    while (steps < 1200) {
        if (x0 >= min_x && x0 < max_x && y0 >= min_y && y0 < max_y) {
            screen_rect(x0, y0, 2, 2, col);
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        steps++;
    }
}

void gui_3d_open(void)  { is_open = 1; }
void gui_3d_close(void) { is_open = 0; }

void gui_3d_update(mouse_state_t m, uint8_t click, uint8_t sc) {
    if (!is_open) return;

    int wx = is_max ? 0 : ex;
    int wy = is_max ? 0 : ey;
    int ww = is_max ? g_scr_w : 540;

    if (click) {
        if (m.x >= wx + ww - 44 && m.x <= wx + ww - 8 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_open = 0; return;
        } else if (m.x >= wx + ww - 86 && m.x <= wx + ww - 50 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_max = !is_max; return;
        } else if (!is_max && m.x >= wx && m.x <= wx + ww && m.y >= wy && m.y <= wy + 38) {
            dragging = 1; dox = m.x - ex; doy = m.y - ey;
        }
    }

    if (!m.left_btn) dragging = 0;
    else if (dragging && !is_max) {
        ex = m.x - dox; ey = m.y - doy;
        if (ex < 0) ex = 0;
        if (ex > g_scr_w - 540) ex = g_scr_w - 540;
        if (ey < 0) ey = 0;
        if (ey > g_scr_h - 440) ey = g_scr_h - 440;
    }

    if (sc != 0) {
        if (sc == 0x39) auto_rot = !auto_rot;
        else if (sc == 0x11 || sc == 0x48) ang_x = (ang_x - 2) & 63;
        else if (sc == 0x1F || sc == 0x50) ang_x = (ang_x + 2) & 63;
        else if (sc == 0x1E || sc == 0x4B) ang_y = (ang_y - 2) & 63;
        else if (sc == 0x20 || sc == 0x4D) ang_y = (ang_y + 2) & 63;
        else if (sc == 0x0D && zoom < 180) zoom += 10;
        else if (sc == 0x0C && zoom > 50)  zoom -= 10;
    }

    if (auto_rot) {
        tick_div++;
        if (tick_div >= 4) {
            tick_div = 0;
            ang_y = (ang_y + 1) & 63;
        }
    }
}

void gui_3d_draw(void) {
    if (!is_open) return;

    int wx = is_max ? 0 : ex;
    int wy = is_max ? 0 : ey;
    int ww = is_max ? g_scr_w : 540;
    int wh = is_max ? (g_scr_h - 44) : 400;

    screen_rect_alpha(wx, wy, ww, wh, 0x101622, 225);
    screen_round_rect(wx, wy, ww, 38, 8, 0x1A237E);
    screen_str(wx + 16, wy + 11, "3D (WASD / Space / +-)", 0xFFFFFF);

    screen_round_rect(wx + ww - 86, wy + 5, 34, 28, 6, 0x2E7D32);
    screen_str(wx + ww - 76, wy + 11, is_max ? "v" : "^", 0xFFFFFF);

    screen_round_rect(wx + ww - 44, wy + 5, 34, 28, 6, 0xC62828);
    screen_str(wx + ww - 34, wy + 11, "X", 0xFFFFFF);

    int cx = wx + ww / 2;
    int cy = wy + 38 + (wh - 38) / 2;
    int scale = is_max ? (zoom + 70) : zoom;

    int sy = *(sin_lut + (ang_y & 63));
    int cy_rot = *(sin_lut + ((ang_y + 16) & 63));
    int sx = *(sin_lut + (ang_x & 63));
    int cx_rot = *(sin_lut + ((ang_x + 16) & 63));

    int px[VERT_COUNT];
    int py[VERT_COUNT];
    for (int i = 0; i < VERT_COUNT; i++) {
        int x1 = (*(cube_vx + i) * cy_rot - *(cube_vz + i) * sy) / 256;
        int z1 = (*(cube_vx + i) * sy + *(cube_vz + i) * cy_rot) / 256;
        int y2 = (*(cube_vy + i) * cx_rot - z1 * sx) / 256;
        int z2 = (*(cube_vy + i) * sx + z1 * cx_rot) / 256;
        int fov = 350 + z2;
        if (fov < 100) fov = 100;
        *(px + i) = cx + (x1 * scale * 3) / fov;
        *(py + i) = cy + (y2 * scale * 3) / fov;
    }

    for (int e = 0; e < EDGE_COUNT; e++) {
        int a = *(edge_a + e);
        int b = *(edge_b + e);
        draw_line(*(px + a), *(py + a), *(px + b), *(py + b), 0x00E5FF, wx + 4, wy + 40, wx + ww - 6, wy + wh - 6);
    }
}