#include "apps.h"
#include "../vfs.h"
#include "../sas_vm.h"
#include "../Drivers/screen/screen.h"
#include "../Drivers/screen/notify.h"

extern void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer);
extern int ata_identify_drive(uint8_t drive, char *model_out);

#define DIR_COUNT 4
#define MODEL_LEN 48
#define SEC_WORDS 256

static int is_open = 0;
static int is_max = 0;
static int cur_dir_idx = 3;
static int fx = 200, fy = 90;
static int dragging = 0, dox = 0, doy = 0;

static const char *dirs[DIR_COUNT] = { "/Sashix", "/Users", "/Apps", "/Media" };

static int f_eq(const char *a, const char *b) {
    int i = 0;
    while (*(a + i) && *(b + i)) {
        if (*(a + i) != *(b + i)) return 0;
        i++;
    }
    return *(a + i) == *(b + i);
}

static int has_ext(const char *name, const char *ext) {
    int nlen = 0, elen = 0;
    while (*(name + nlen)) nlen++;
    while (*(ext + elen)) elen++;
    if (nlen < elen) return 0;
    for (int i = 0; i < elen; i++) {
        if (*(name + nlen - elen + i) != *(ext + i)) return 0;
    }
    return 1;
}

static void import_from_usb_slave(void) {
    char model[MODEL_LEN];
    if (!ata_identify_drive(1, model)) {
        notify_push("USB", "No drive");
        return;
    }
    uint16_t sec[SEC_WORDS];
    ata_read_sector_drv(1, 0, sec);
    uint8_t *raw = (uint8_t *)sec;
    if (*(raw + 0) == 'S' && *(raw + 1) == 'A' && *(raw + 2) == 'S' && *(raw + 3) == 'H') {
        vfs_write_file("/Apps", "usb_app.sas", VFS_TYPE_APP, 0, raw, 512);
        notify_push("USB", "Saved /Apps/usb_app.sas");
    } else {
        notify_push("USB", "No .sas found");
    }
}

void gui_files_init(void)  { is_open = 0; is_max = 0; }
void gui_files_open(void)  { is_open = 1; }
void gui_files_close(void) { is_open = 0; }

void gui_files_update(mouse_state_t m, uint8_t click) {
    if (!is_open) return;

    int wx = is_max ? 0 : fx;
    int wy = is_max ? 0 : fy;
    int ww = is_max ? g_scr_w : 700;
    int wh = is_max ? (g_scr_h - 44) : 480;

    if (click) {
        if (m.x >= wx + ww - 44 && m.x <= wx + ww - 8 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_open = 0; return;
        } else if (m.x >= wx + ww - 86 && m.x <= wx + ww - 50 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_max = !is_max; return;
        } else if (!is_max && m.x >= wx && m.x <= wx + ww && m.y >= wy && m.y <= wy + 38) {
            dragging = 1; dox = m.x - fx; doy = m.y - fy;
        } else if (m.x >= wx + 12 && m.x <= wx + 154 && m.y >= wy + 50 && m.y <= wy + 250) {
            int idx = (m.y - (wy + 50)) / 50;
            if (idx >= 0 && idx < DIR_COUNT) cur_dir_idx = idx;
        } else if (m.x >= wx + 12 && m.x <= wx + 154 && m.y >= wy + wh - 54 && m.y <= wy + wh - 14) {
            import_from_usb_slave();
        } else if (m.x >= wx + 170 && m.x <= wx + ww - 16 && m.y >= wy + 50 && m.y <= wy + wh - 16) {
            int row = (m.y - (wy + 50)) / 36;
            int found = 0;
            for (int i = 0; i < vfs_count(); i++) {
                vfs_node_t *n = vfs_get_node(i);
                if (n && f_eq(n->dir, *(dirs + cur_dir_idx))) {
                    if (found == row) {
                        if (m.x >= wx + ww - 60) {
                            vfs_delete_file(n->dir, n->name);
                        } else if (has_ext(n->name, ".sas")) {
                            sas_exec_node(n);
                        } else if (has_ext(n->name, ".jpg") || has_ext(n->name, ".mp4")) {
                            gui_media_open_node(n->name, n->data, n->size);
                        } else {
                            notify_push(n->name, (const char *)n->data);
                        }
                        break;
                    }
                    found++;
                }
            }
        }
    }

    if (!m.left_btn) dragging = 0;
    else if (dragging && !is_max) {
        fx = m.x - dox; fy = m.y - doy;
        if (fx < 0) fx = 0;
        if (fx > g_scr_w - 700) fx = g_scr_w - 700;
        if (fy < 0) fy = 0;
        if (fy > g_scr_h - 520) fy = g_scr_h - 520;
    }
}

void gui_files_draw(void) {
    if (!is_open) return;

    int wx = is_max ? 0 : fx;
    int wy = is_max ? 0 : fy;
    int ww = is_max ? g_scr_w : 700;
    int wh = is_max ? (g_scr_h - 44) : 480;

    screen_rect_alpha(wx, wy, ww, wh, 0x161B24, 225);
    screen_round_rect(wx, wy, ww, 38, 8, 0x24344D);

    screen_str(wx + 16, wy + 11, "SashixFS", 0xFFFFFF);
    screen_str(wx + 240, wy + 11, vfs_is_root() ? "[ROOT]" : "[USER]", vfs_is_root() ? 0xFF5555 : 0x55FF55);

    screen_round_rect(wx + ww - 86, wy + 5, 34, 28, 6, 0x2E7D32);
    screen_str(wx + ww - 76, wy + 11, is_max ? "v" : "^", 0xFFFFFF);

    screen_round_rect(wx + ww - 44, wy + 5, 34, 28, 6, 0xC62828);
    screen_str(wx + ww - 34, wy + 11, "X", 0xFFFFFF);

    screen_rect_alpha(wx + 8, wy + 44, 150, wh - 52, 0x10141C, 210);
    for (int i = 0; i < DIR_COUNT; i++) {
        int dy = wy + 50 + i * 50;
        screen_round_rect(wx + 12, dy, 142, 42, 6, cur_dir_idx == i ? 0x2B579A : 0x222630);
        screen_str(wx + 20, dy + 13, *(dirs + i), 0xFFFFFF);
    }

    screen_round_rect(wx + 12, wy + wh - 54, 142, 40, 6, 0x006644);
    screen_str(wx + 20, wy + wh - 42, "USB", 0xFFFFFF);

    int row = 0;
    for (int i = 0; i < vfs_count(); i++) {
        vfs_node_t *n = vfs_get_node(i);
        if (n && f_eq(n->dir, *(dirs + cur_dir_idx))) {
            int ry = wy + 50 + row * 36;
            if (ry + 32 > wy + wh - 12) break;
            screen_round_rect(wx + 170, ry, ww - 186, 32, 6, 0x262B36);
            uint32_t col = has_ext(n->name, ".sas") ? 0x00FFAA : (n->is_system ? 0xFFCC44 : 0xFFFFFF);
            screen_str(wx + 180, ry + 8, n->name, col);

            screen_round_rect(wx + ww - 56, ry + 2, 36, 28, 6, 0x992222);
            screen_str(wx + ww - 46, ry + 8, "X", 0xFFFFFF);
            row++;
        }
    }
}