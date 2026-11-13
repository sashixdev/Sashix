#include "apps.h"
#include "../Drivers/screen/screen.h"

extern void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer);

#define IMG_W      128
#define IMG_H      96
#define SEC_COUNT  48
#define TITLE_MAX  32

static int is_open = 0;
static int is_max = 0;
static int is_video = 0;
static uint32_t base_lba = 300;
static uint16_t total_frames = 1;
static uint16_t cur_frame = 0;
static int frame_div = 0;

static char title_buf[TITLE_MAX];
static uint16_t *frame_buf = (uint16_t *)0x01900000;

static int mx = 240, my = 110;
static int dragging = 0, dox = 0, doy = 0;

static void m_cpy(char *dst, const char *src, int max) {
    int i = 0;
    while (*(src + i) && i < max - 1) {
        *(dst + i) = *(src + i);
        i++;
    }
    *(dst + i) = '\0';
}

static void load_frame_from_disk(uint32_t lba) {
    uint16_t *dst = frame_buf;
    for (uint32_t s = 0; s < SEC_COUNT; s++) {
        ata_read_sector_drv(0, lba + s, dst + s * 256);
    }
}

void gui_media_open_jpg(const char *name, uint32_t lba) {
    m_cpy(title_buf, name, TITLE_MAX);
    base_lba = lba;
    total_frames = 1;
    cur_frame = 0;
    is_video = 0;
    load_frame_from_disk(base_lba);
    is_open = 1;
}

void gui_media_open_mp4(const char *name, uint32_t lba, uint16_t frames) {
    m_cpy(title_buf, name, TITLE_MAX);
    base_lba = lba;
    total_frames = (frames > 0) ? frames : 1;
    cur_frame = 0;
    frame_div = 0;
    is_video = 1;
    load_frame_from_disk(base_lba);
    is_open = 1;
}

void gui_media_open_node(const char *name, const uint8_t *data, uint32_t size) {
    if (size >= 12 && *(data + 0) == 'M' && *(data + 1) == 'E' && *(data + 2) == 'D') {
        uint8_t kind = *(data + 3);
        uint32_t lba = *(const uint32_t *)(data + 4);
        uint32_t fcnt = *(const uint32_t *)(data + 8);
        if (kind == 2) gui_media_open_mp4(name, lba, (uint16_t)fcnt);
        else gui_media_open_jpg(name, lba);
    } else {
        gui_media_open_jpg(name, 300);
    }
}

void gui_media_close(void) {
    is_open = 0;
    is_video = 0;
}

void gui_media_update(mouse_state_t m, uint8_t click) {
    if (!is_open) return;

    int wx = is_max ? 0 : mx;
    int wy = is_max ? 0 : my;
    int ww = is_max ? g_scr_w : 580;

    if (click) {
        if (m.x >= wx + ww - 44 && m.x <= wx + ww - 8 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_open = 0;
            is_video = 0;
            return;
        } else if (m.x >= wx + ww - 86 && m.x <= wx + ww - 50 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_max = !is_max;
            return;
        } else if (!is_max && m.x >= wx && m.x <= wx + ww && m.y >= wy && m.y <= wy + 38) {
            dragging = 1;
            dox = m.x - mx;
            doy = m.y - my;
        }
    }

    if (!m.left_btn) dragging = 0;
    else if (dragging && !is_max) {
        mx = m.x - dox;
        my = m.y - doy;
        if (mx < 0) mx = 0;
        if (mx > g_scr_w - 580) mx = g_scr_w - 580;
        if (my < 0) my = 0;
        if (my > g_scr_h - 490) my = g_scr_h - 490;
    }

    if (is_video && total_frames > 1) {
        frame_div++;
        if (frame_div >= 6) {
            frame_div = 0;
            cur_frame++;
            if (cur_frame >= total_frames) cur_frame = 0;
            load_frame_from_disk(base_lba + (uint32_t)cur_frame * SEC_COUNT);
        }
    }
}

void gui_media_draw(void) {
    if (!is_open) return;

    int wx = is_max ? 0 : mx;
    int wy = is_max ? 0 : my;
    int ww = is_max ? g_scr_w : 580;
    int wh = is_max ? (g_scr_h - 44) : 450;

    screen_rect_alpha(wx, wy, ww, wh, 0x12161F, 225);
    screen_round_rect(wx, wy, ww, 38, 8, 0x24344D);
    screen_str(wx + 16, wy + 11, title_buf, 0xFFFFFF);

    screen_round_rect(wx + ww - 86, wy + 5, 34, 28, 6, 0x2E7D32);
    screen_str(wx + ww - 76, wy + 11, is_max ? "v" : "^", 0xFFFFFF);

    screen_round_rect(wx + ww - 44, wy + 5, 34, 28, 6, 0xC62828);
    screen_str(wx + ww - 34, wy + 11, "X", 0xFFFFFF);

    int scale = is_max ? 6 : 4;
    int px0 = wx + (ww - IMG_W * scale) / 2;
    int py0 = wy + 46 + ((wh - 54) - IMG_H * scale) / 2;

    for (int y = 0; y < IMG_H; y++) {
        for (int x = 0; x < IMG_W; x++) {
            uint16_t c16 = *(frame_buf + y * IMG_W + x);
            uint32_t r = ((c16 >> 11) & 0x1F) << 3;
            uint32_t g = ((c16 >> 5)  & 0x3F) << 2;
            uint32_t b = (c16 & 0x1F) << 3;
            screen_rect(px0 + x * scale, py0 + y * scale, scale, scale, (r << 16) | (g << 8) | b);
        }
    }
}