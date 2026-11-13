#include "apps.h"
#include "../Drivers/disk/io.h"
#include "../Drivers/screen/screen.h"
#include "../Drivers/screen/notify.h"

extern void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer);
extern void ata_write_sector_drv(uint8_t drive, uint32_t lba, const uint16_t *source_buffer);

#define CFG_LBA   145
#define CFG_WORDS 256
#define BG_COUNT  4

sys_settings_t g_settings = { 3, 0x1A2B3C, 100, 80 };

static int is_open = 0;
static int is_max = 0;
static int sx = 240, sy = 100;
static int dragging = 0, dox = 0, doy = 0;

static const uint32_t bg_colors[BG_COUNT] = { 0x1A2B3C, 0x1E293B, 0x2D1B36, 0x132A1E };

static void save_settings(void) {
    uint16_t sec[CFG_WORDS];
    for (int i = 0; i < CFG_WORDS; i++) *(sec + i) = 0;
    uint8_t *raw = (uint8_t *)sec;
    *(raw + 0) = 'C'; *(raw + 1) = 'F'; *(raw + 2) = 'G'; *(raw + 3) = '1';
    *(int *)(raw + 4) = g_settings.tz_offset;
    *(uint32_t *)(raw + 8) = g_settings.bg_color;
    *(raw + 12) = g_settings.brightness;
    *(raw + 13) = g_settings.volume;
    ata_write_sector_drv(0, CFG_LBA, sec);
}

void gui_settings_init(void) {
    is_open = 0; is_max = 0;
    uint16_t sec[CFG_WORDS];
    ata_read_sector_drv(0, CFG_LBA, sec);
    uint8_t *raw = (uint8_t *)sec;
    if (*(raw + 0) == 'C' && *(raw + 1) == 'F' && *(raw + 2) == 'G' && *(raw + 3) == '1') {
        g_settings.tz_offset = *(int *)(raw + 4);
        g_settings.bg_color  = *(uint32_t *)(raw + 8);
        g_settings.brightness = *(raw + 12);
        g_settings.volume     = *(raw + 13);
    }
    screen_set_brightness(g_settings.brightness);
}

void gui_settings_open(void)  { is_open = 1; }
void gui_settings_close(void) { is_open = 0; }

void gui_settings_update(mouse_state_t m, uint8_t click) {
    if (!is_open) return;

    int wx = is_max ? 0 : sx;
    int wy = is_max ? 0 : sy;
    int ww = is_max ? g_scr_w : 560;

    if (click) {
        if (m.x >= wx + ww - 44 && m.x <= wx + ww - 8 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_open = 0; return;
        } else if (m.x >= wx + ww - 86 && m.x <= wx + ww - 50 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_max = !is_max; return;
        } else if (!is_max && m.x >= wx && m.x <= wx + ww && m.y >= wy && m.y <= wy + 38) {
            dragging = 1; dox = m.x - sx; doy = m.y - sy;
        } else if (m.x >= wx + 240 && m.x <= wx + 290 && m.y >= wy + 60 && m.y <= wy + 96) {
            if (g_settings.tz_offset > -12) g_settings.tz_offset--;
            save_settings();
        } else if (m.x >= wx + 360 && m.x <= wx + 410 && m.y >= wy + 60 && m.y <= wy + 96) {
            if (g_settings.tz_offset < 14) g_settings.tz_offset++;
            save_settings();
        } else if (m.x >= wx + 240 && m.x <= wx + 290 && m.y >= wy + 120 && m.y <= wy + 156) {
            if (g_settings.brightness > 30) g_settings.brightness -= 10;
            screen_set_brightness(g_settings.brightness);
            save_settings();
        } else if (m.x >= wx + 360 && m.x <= wx + 410 && m.y >= wy + 120 && m.y <= wy + 156) {
            if (g_settings.brightness < 100) g_settings.brightness += 10;
            screen_set_brightness(g_settings.brightness);
            save_settings();
        } else if (m.x >= wx + 240 && m.x <= wx + 290 && m.y >= wy + 180 && m.y <= wy + 216) {
            if (g_settings.volume >= 10) g_settings.volume -= 10;
            save_settings();
        } else if (m.x >= wx + 360 && m.x <= wx + 410 && m.y >= wy + 180 && m.y <= wy + 216) {
            if (g_settings.volume <= 90) g_settings.volume += 10;
            save_settings();
        } else if (m.y >= wy + 250 && m.y <= wy + 294) {
            for (int i = 0; i < BG_COUNT; i++) {
                int bx = wx + 200 + i * 70;
                if (m.x >= bx && m.x <= bx + 54) {
                    g_settings.bg_color = *(bg_colors + i);
                    save_settings();
                }
            }
        }
    }

    if (!m.left_btn) dragging = 0;
    else if (dragging && !is_max) {
        sx = m.x - dox; sy = m.y - doy;
        if (sx < 0) sx = 0;
        if (sx > g_scr_w - 560) sx = g_scr_w - 560;
        if (sy < 0) sy = 0;
        if (sy > g_scr_h - 420) sy = g_scr_h - 420;
    }
}

void gui_settings_draw_bg(void) {
    screen_clear(g_settings.bg_color);
}

static void print_val(int x, int y, int val) {
    char buf[16];
    int p = 0;
    if (val < 0) { *(buf + p++) = '-'; val = -val; }
    if (val >= 100) { *(buf + p++) = (char)('0' + (val / 100) % 10); }
    if (val >= 10)  { *(buf + p++) = (char)('0' + (val / 10) % 10); }
    *(buf + p++) = (char)('0' + (val % 10));
    *(buf + p) = '\0';
    screen_str(x, y, buf, 0x00FFAA);
}

void gui_settings_draw(void) {
    if (!is_open) return;

    int wx = is_max ? 0 : sx;
    int wy = is_max ? 0 : sy;
    int ww = is_max ? g_scr_w : 560;
    int wh = is_max ? (g_scr_h - 44) : 360;

    screen_rect_alpha(wx, wy, ww, wh, 0x161B24, 225);
    screen_round_rect(wx, wy, ww, 38, 8, 0x37474F);
    screen_str(wx + 16, wy + 11, "Settings", 0xFFFFFF);

    screen_round_rect(wx + ww - 86, wy + 5, 34, 28, 6, 0x2E7D32);
    screen_str(wx + ww - 76, wy + 11, is_max ? "v" : "^", 0xFFFFFF);

    screen_round_rect(wx + ww - 44, wy + 5, 34, 28, 6, 0xC62828);
    screen_str(wx + ww - 34, wy + 11, "X", 0xFFFFFF);

    screen_str(wx + 24, wy + 68, "UTC:", 0xFFFFFF);
    screen_round_rect(wx + 240, wy + 60, 50, 36, 6, 0x2B579A);
    screen_str(wx + 258, wy + 69, "-", 0xFFFFFF);
    print_val(wx + 302, wy + 69, g_settings.tz_offset);
    screen_round_rect(wx + 360, wy + 60, 50, 36, 6, 0x2B579A);
    screen_str(wx + 378, wy + 69, "+", 0xFFFFFF);

    screen_str(wx + 24, wy + 128, "Bright:", 0xFFFFFF);
    screen_round_rect(wx + 240, wy + 120, 50, 36, 6, 0x2B579A);
    screen_str(wx + 258, wy + 129, "-", 0xFFFFFF);
    print_val(wx + 302, wy + 129, g_settings.brightness);
    screen_round_rect(wx + 360, wy + 120, 50, 36, 6, 0x2B579A);
    screen_str(wx + 378, wy + 129, "+", 0xFFFFFF);

    screen_str(wx + 24, wy + 188, "Sound:", 0xFFFFFF);
    screen_round_rect(wx + 240, wy + 180, 50, 36, 6, 0x2B579A);
    screen_str(wx + 258, wy + 189, "-", 0xFFFFFF);
    print_val(wx + 302, wy + 189, g_settings.volume);
    screen_round_rect(wx + 360, wy + 180, 50, 36, 6, 0x2B579A);
    screen_str(wx + 378, wy + 189, "+", 0xFFFFFF);

    screen_str(wx + 24, wy + 262, "Theme:", 0xFFFFFF);
    for (int i = 0; i < BG_COUNT; i++) {
        screen_round_rect(wx + 200 + i * 70, wy + 250, 54, 44, 8, *(bg_colors + i));
    }
}