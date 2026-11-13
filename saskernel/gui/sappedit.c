#include "apps.h"
#include "../vfs.h"
#include "../sas_vm.h"
#include "../Drivers/screen/screen.h"
#include "../Drivers/screen/notify.h"
#include "../Drivers/keyboard/keyboard.h"

#define CODE_MAX 1024
#define NAME_MAX 20
#define LINE_MAX 54

static int is_open = 0;
static int is_max = 0;
static int sx = 150, sy = 70;
static int dragging = 0, dox = 0, doy = 0;

static char out_name[NAME_MAX] = "demo.sas";
static int name_len = 8;
static int edit_name = 0;

static char code_buf[CODE_MAX];
static int code_len = 0;

static void se_cpy(char *dst, const char *src, int max) {
    int i = 0;
    while (*(src + i) && i < max - 1) {
        *(dst + i) = *(src + i);
        i++;
    }
    *(dst + i) = '\0';
}

void gui_sappedit_init(void) {
    is_open = 0;
    is_max = 0;
    const char *tpl =
        "// Sapp Edit\n"
        "notify(\"Sashix\", \"Hello .sas!\");\n"
        "beep(880);\n";
    se_cpy(code_buf, tpl, CODE_MAX);
    code_len = 0;
    while (*(code_buf + code_len)) code_len++;
}

void gui_sappedit_open(void)  { is_open = 1; }
void gui_sappedit_close(void) { is_open = 0; }

void gui_sappedit_copy_clip(void) {
    if (!is_open) return;
    se_cpy(g_clipboard, code_buf, 1024);
    notify_push("Clipboard", "Code copied (Ctrl+A)");
}

void gui_sappedit_paste_clip(void) {
    if (!is_open) return;
    int i = 0;
    while (*(g_clipboard + i) && code_len < CODE_MAX - 1) {
        *(code_buf + (code_len++)) = *(g_clipboard + i++);
    }
    *(code_buf + code_len) = '\0';
}

static void build_and_run(int run_after) {
    if (name_len == 0) {
        se_cpy(out_name, "app.sas", NAME_MAX);
        name_len = 7;
    }
    int res = sas_compile_text(code_buf, (uint32_t)code_len, out_name);
    if (res == 0) {
        notify_push("Sapp Edit", "Build OK -> /Apps");
        if (run_after) {
            vfs_node_t *n = vfs_find("/Apps", out_name);
            if (n) sas_exec_node(n);
        }
    } else {
        notify_push("Sapp Edit", "Build Error");
    }
}

void gui_sappedit_update(mouse_state_t m, uint8_t click, uint8_t sc, uint8_t ctrl, uint8_t shift) {
    if (!is_open) return;

    int wx = is_max ? 0 : sx;
    int wy = is_max ? 0 : sy;
    int ww = is_max ? g_scr_w : 780;

    if (click) {
        if (m.x >= wx + ww - 44 && m.x <= wx + ww - 8 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_open = 0; return;
        } else if (m.x >= wx + ww - 86 && m.x <= wx + ww - 50 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_max = !is_max; return;
        } else if (!is_max && m.x >= wx && m.x <= wx + ww && m.y >= wy && m.y <= wy + 38) {
            dragging = 1; dox = m.x - sx; doy = m.y - sy;
        } else if (m.x >= wx + 16 && m.x <= wx + 256 && m.y >= wy + 44 && m.y <= wy + 78) {
            edit_name = 1;
        } else if (m.x >= wx + 270 && m.x <= wx + 450 && m.y >= wy + 44 && m.y <= wy + 78) {
            edit_name = 0; build_and_run(0);
        } else if (m.x >= wx + 464 && m.x <= wx + 664 && m.y >= wy + 44 && m.y <= wy + 78) {
            edit_name = 0; build_and_run(1);
        } else {
            edit_name = 0;
        }
    }

    if (!m.left_btn) dragging = 0;
    else if (dragging && !is_max) {
        sx = m.x - dox; sy = m.y - doy;
        if (sx < 0) sx = 0;
        if (sx > g_scr_w - 780) sx = g_scr_w - 780;
        if (sy < 0) sy = 0;
        if (sy > g_scr_h - 540) sy = g_scr_h - 540;
    }

    if (!sc) return;

    if (ctrl && sc == 0x1E) { gui_sappedit_copy_clip(); return; }
    if (ctrl && sc == 0x1F) { gui_sappedit_paste_clip(); return; }

    if (sc == 0x3F) {
        build_and_run(1);
        return;
    }

    if (edit_name) {
        if (sc == 0x0E && name_len > 0) {
            *(out_name + (--name_len)) = '\0';
        } else if (sc == 0x1C) {
            edit_name = 0;
        } else {
            const char *utf = keyboard_scancode_to_utf8(sc, shift);
            if (*utf >= 33 && *utf <= 126 && name_len < NAME_MAX - 1) {
                *(out_name + (name_len++)) = *utf;
                *(out_name + name_len) = '\0';
            }
        }
        return;
    }

    if (sc == 0x0E && code_len > 0) {
        code_len--;
        if (code_len > 0 && ((uint8_t)*(code_buf + code_len) & 0xC0) == 0x80) code_len--;
        *(code_buf + code_len) = '\0';
    } else if (sc == 0x1C && code_len < CODE_MAX - 1) {
        *(code_buf + (code_len++)) = '\n';
        *(code_buf + code_len) = '\0';
    } else {
        const char *utf = keyboard_scancode_to_utf8(sc, shift);
        int k = 0;
        while (*(utf + k) && code_len < CODE_MAX - 1) {
            *(code_buf + (code_len++)) = *(utf + k++);
        }
        *(code_buf + code_len) = '\0';
    }
}

void gui_sappedit_draw(void) {
    if (!is_open) return;

    int wx = is_max ? 0 : sx;
    int wy = is_max ? 0 : sy;
    int ww = is_max ? g_scr_w : 780;
    int wh = is_max ? (g_scr_h - 44) : 510;

    screen_rect_alpha(wx, wy, ww, wh, 0x141820, 225);
    screen_round_rect(wx, wy, ww, 38, 8, 0x5E35B1);
    screen_str(wx + 16, wy + 11, "Sapp Edit", 0xFFFFFF);

    screen_round_rect(wx + ww - 86, wy + 5, 34, 28, 6, 0x2E7D32);
    screen_str(wx + ww - 76, wy + 11, is_max ? "v" : "^", 0xFFFFFF);

    screen_round_rect(wx + ww - 44, wy + 5, 34, 28, 6, 0xC62828);
    screen_str(wx + ww - 34, wy + 11, "X", 0xFFFFFF);

    screen_round_rect(wx + 16, wy + 44, 240, 34, 6, edit_name ? 0x2B3445 : 0x1E2430);
    screen_str(wx + 24, wy + 53, out_name, 0x00FFAA);

    screen_round_rect(wx + 270, wy + 44, 180, 34, 6, 0x0066CC);
    screen_str(wx + 292, wy + 53, "Build", 0xFFFFFF);

    screen_round_rect(wx + 464, wy + 44, 200, 34, 6, 0x008844);
    screen_str(wx + 492, wy + 53, "Run (F5)", 0xFFFFFF);

    screen_rect_alpha(wx + 16, wy + 86, ww - 32, wh - 98, 0x0B0E14, 215);

    int rx = wx + 26, ry = wy + 96;
    char lbuf[LINE_MAX];
    int lp = 0;
    int max_c = (ww - 56) / 16;
    if (max_c >= LINE_MAX) max_c = LINE_MAX - 1;

    for (int i = 0; i <= code_len; i++) {
        char c = *(code_buf + i);
        if (c == '\n' || c == '\0' || lp >= max_c) {
            *(lbuf + lp) = '\0';
            if (ry + 20 < wy + wh - 16) {
                uint32_t col = (*(lbuf + 0) == '/' && *(lbuf + 1) == '/') ? 0x6A9955 : 0xE0E0E0;
                screen_str(rx, ry, lbuf, col);
                ry += 22;
            }
            lp = 0;
            if (c == '\0') break;
        } else {
            *(lbuf + lp++) = c;
        }
    }
}