#include "apps.h"
#include "../vfs.h"
#include "../Drivers/disk/types.h"
#include "../Drivers/disk/io.h"
#include "../Drivers/screen/screen.h"
#include "../Drivers/screen/mouse.h"
#include "../Drivers/screen/notify.h"
#include "../Drivers/keyboard/keyboard.h"

extern void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer);
extern void ata_write_sector_drv(uint8_t drive, uint32_t lba, const uint16_t *source_buffer);
extern uint8_t g_sys_lang;
extern uint8_t g_kbd_lang;

#define TIME_STR_LEN 6
#define HEX_STR_LEN  11
#define SEC_WORDS    256
#define CRED_LEN     8
#define MAX_DISK_LBA 20400
#define CLIP_MAX     1024
#define DBG_MAX      48
#define META_SIZE    12
#define NAME_LEN     24
#define FPS_LEN      16

char g_clipboard[CLIP_MAX] = "";

static int is_locked = 0;
static char sys_user[CRED_LEN] = "sasha";
static char sys_pass[CRED_LEN] = "";
static char pass_input[CRED_LEN] = "";
static char pass_stars[CRED_LEN] = "";
static int pass_len = 0;

static uint32_t next_media_lba = 300;
static int shot_idx = 1;
static int rec_idx = 1;

static char dbg_msg[DBG_MAX] = "DEV DEBUG: Idle";

static void dbg_log(const char *s) {
    int p = 0;
    const char *pref = "DBG: ";
    while (pref[p]) {
        dbg_msg[p] = pref[p];
        p++;
    }
    int i = 0;
    while (s[i] && p < DBG_MAX - 2) {
        dbg_msg[p] = s[i];
        p++;
        i++;
    }
    dbg_msg[p] = '\0';
}

static void make_fps_str(int fps, char *out) {
    out[0] = 'F';
    out[1] = 'P';
    out[2] = 'S';
    out[3] = ':';
    out[4] = ' ';
    int p = 5;
    if (fps >= 100) out[p++] = (char)('0' + (fps / 100) % 10);
    if (fps >= 10)  out[p++] = (char)('0' + (fps / 10) % 10);
    out[p++] = (char)('0' + (fps % 10));
    out[p] = '\0';
}

static void make_media_name(char *out, const char *pref, int num, const char *ext) {
    int p = 0;
    while (pref[p]) { out[p] = pref[p]; p++; }
    if (num >= 10) out[p++] = (char)('0' + (num / 10) % 10);
    out[p++] = (char)('0' + (num % 10));
    int e = 0;
    while (ext[e]) out[p++] = ext[e++];
    out[p] = '\0';
}

static void save_media_meta(const char *name, uint8_t kind, uint32_t lba, uint32_t frames) {
    uint8_t meta[META_SIZE];
    meta[0] = 'M';
    meta[1] = 'E';
    meta[2] = 'D';
    meta[3] = kind;
    *(uint32_t *)(meta + 4) = lba;
    *(uint32_t *)(meta + 8) = frames;
    vfs_write_file("/Media", name, VFS_TYPE_FILE, 0, meta, META_SIZE);
}

static void load_user_credentials(void) {
    uint16_t mbr[SEC_WORDS];
    ata_read_sector_drv(0, 0, mbr);
    uint8_t *r = (uint8_t *)mbr;
    if (r[490] == 'S' && r[491] == 'A' && r[492] == 'S' && r[493] == 'H') {
        int i = 0;
        while (r[495 + i] && i < CRED_LEN - 1) {
            sys_user[i] = (char)r[495 + i];
            i++;
        }
        sys_user[i] = '\0';

        i = 0;
        while (r[503 + i] && i < CRED_LEN - 1) {
            sys_pass[i] = (char)r[503 + i];
            i++;
        }
        sys_pass[i] = '\0';
        if (i > 0) is_locked = 1;
    }
}

static int str_equal(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void sys_poweroff(void) {
    asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
    asm volatile ("cli; hlt");
}

static void sys_reboot(void) {
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);
    asm volatile ("hlt");
}

static void capture_frame_to_disk(uint32_t lba) {
    uint32_t *src = (uint32_t *)0x01000000;
    uint16_t sec[SEC_WORDS];
    int p = 0, s = 0;

    for (int iy = 0; iy < 96; iy++) {
        int sy = (iy * g_scr_h) / 96;
        for (int ix = 0; ix < 128; ix++) {
            int sx = (ix * g_scr_w) / 128;
            uint32_t col = src[sy * g_scr_w + sx];
            uint16_t r = (col >> 19) & 0x1F;
            uint16_t g = (col >> 10) & 0x3F;
            uint16_t b = (col >> 3)  & 0x1F;
            sec[p] = (r << 11) | (g << 5) | b;
            p++;
            if (p == 256) {
                ata_write_sector_drv(0, lba + s, sec);
                s++;
                p = 0;
            }
        }
    }
}

static void to_hex(uint32_t val, char *out) {
    const char *d = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 8; i++) out[9 - i] = d[(val >> (i * 4)) & 0x0F];
    out[10] = '\0';
}

static uint8_t read_rtc(uint8_t reg) {
    outb(0x70, reg);
    uint8_t val = inb(0x71);
    return (val & 0x0F) + ((val / 16) * 10);
}

static void get_rtc_time(char *buf) {
    int h = ((int)read_rtc(0x04) + g_settings.tz_offset) % 24;
    if (h < 0) h += 24;
    uint8_t m = read_rtc(0x02);
    buf[0] = '0' + (h / 10);
    buf[1] = '0' + (h % 10);
    buf[2] = ':';
    buf[3] = '0' + (m / 10);
    buf[4] = '0' + (m % 10);
    buf[5] = '\0';
}

static void draw_lock_screen(const char *time_str) {
    int cx = g_scr_w / 2;
    int cy = g_scr_h / 2;
    screen_clear(0x0D1117);
    screen_round_rect(cx - 200, cy - 180, 400, 380, 12, 0x161B22);
    screen_round_rect(cx - 200, cy - 180, 400, 44,  12, 0x2B579A);

    screen_str(cx - 40, cy - 230, time_str, 0xFFFFFF);
    screen_str(cx - 176, cy - 166, g_sys_lang ? "БЛОКИРОВКА SASHIX" : "SASHIX LOCKED", 0xFFFFFF);

    screen_round_rect(cx - 36, cy - 110, 72, 72, 14, 0x2B579A);
    screen_str(cx - 16, cy - 82, ">_", 0x00FF00);

    screen_str(cx - 168, cy - 12, g_sys_lang ? "Пользователь:" : "User:", 0x8B949E);
    screen_str(cx + 48, cy - 12, sys_user, 0x58A6FF);

    screen_round_rect(cx - 168, cy + 28, 336, 44, 8, 0x0D1117);
    screen_str(cx - 154, cy + 42, pass_stars, 0x00FF00);
    screen_str(cx - 154 + pass_len * 16, cy + 42, "_", 0xFFFFFF);

    screen_round_rect(cx - 120, cy + 96, 240, 46, 8, 0x238636);
    screen_str(cx - 56, cy + 111, g_sys_lang ? "ВОЙТИ" : "LOGIN", 0xFFFFFF);

    screen_str(cx - 176, cy + 164, g_kbd_lang ? "Раскладка: РУС (F6)" : "Layout: ENG (F6)", 0x8B949E);
}

void vga_init(void) {
    uint8_t dev_mode = *(volatile uint8_t *)0x0500;

    screen_init();
    mouse_init();
    keyboard_init();
    vfs_init();
    vfs_check_integrity();
    load_user_credentials();
    gui_settings_init();
    gui_term_init();
    gui_files_init();
    gui_sappedit_init();

    if (!is_locked) {
        if (dev_mode == 1) notify_push("Dev Mode", "FPS Monitor Active");
        else if (dev_mode == 2) notify_push("Dev Mode", "Debug Action Log Active");
        else notify_push("Sashix", g_sys_lang ? "Система готова (ПКМ - меню)" : "Ready (Right-Click menu)");
    }

    int menu_open = 0;
    int ctx_open = 0, ctx_x = 0, ctx_y = 0;
    uint8_t prev_click = 0;
    uint8_t prev_rclick = 0;
    char time_str[TIME_STR_LEN];
    char hb[HEX_STR_LEN];

    uint8_t k_ctrl = 0, k_shift = 0, k_alt = 0, k_win = 0;
    uint8_t key_c = 0, key_a = 0, key_x = 0, key_d = 0;
    uint8_t ext_code = 0;
    uint8_t alt_used = 0;

    int reg_win = 0;
    uint32_t r_eax = 0, r_ebx = 0, r_ecx = 0, r_edx = 0, r_esp = 0, r_ebp = 0;
    int recording = 0;
    uint32_t rec_start_lba = 0;
    uint16_t rec_frames = 0;
    int rec_div = 0;

    int fps_count = 0;
    int fps_val = 0;
    uint8_t last_sec = read_rtc(0x00);
    char fps_str[FPS_LEN];
    make_fps_str(0, fps_str);

    while (1) {
        mouse_poll();
        mouse_state_t m = mouse_get_state();
        uint8_t click = (m.left_btn && !prev_click);
        uint8_t rclick = (m.right_btn && !prev_rclick);
        prev_click = m.left_btn;
        prev_rclick = m.right_btn;

        uint8_t sc = 0;
        while (inb(0x64) & 1) {
            uint8_t status = inb(0x64);
            if (status & 0x20) break;
            uint8_t raw = inb(0x60);

            if (raw == 0xE0) { ext_code = 1; continue; }

            uint8_t released = (raw & 0x80) ? 1 : 0;
            uint8_t code = raw & 0x7F;

            if (ext_code) {
                ext_code = 0;
                if (code == 0x5B || code == 0x5C) k_win = !released;
                continue;
            }

            if (code == 0x1D) k_ctrl  = !released;
            else if (code == 0x2A || code == 0x36) k_shift = !released;
            else if (code == 0x38) {
                if (!released) {
                    if (!k_alt) alt_used = 0;
                    k_alt = 1;
                } else {
                    if (k_alt && !alt_used && !is_locked) {
                        menu_open = !menu_open;
                        dbg_log(menu_open ? "Alt: Start Menu Open" : "Alt: Start Menu Close");
                    }
                    k_alt = 0;
                }
                continue;
            } else if (code == 0x2E) key_c = !released;
            else if (code == 0x1E) key_a = !released;
            else if (code == 0x2D) key_x = !released;
            else if (code == 0x20) key_d = !released;

            if (!released) {
                if (k_alt) alt_used = 1;

                if (code == 0x40) {
                    keyboard_toggle_lang();
                    dbg_log(g_kbd_lang ? "F6: Layout RUS" : "F6: Layout ENG");
                    continue;
                }

                if (k_alt && key_d && key_x) {
                    dbg_log("Alt+D+X: Rebooting");
                    sys_reboot();
                }

                if (is_locked) {
                    if (code == 0x0E && pass_len > 0) {
                        pass_len--;
                        pass_input[pass_len] = '\0';
                        pass_stars[pass_len] = '\0';
                    } else if (code == 0x1C) {
                        if (str_equal(pass_input, sys_pass)) {
                            is_locked = 0;
                            pass_len = 0;
                            pass_input[0] = '\0';
                            pass_stars[0] = '\0';
                            notify_push("Sashix", sys_user);
                            dbg_log("Auth: Login Success");
                        } else {
                            pass_len = 0;
                            pass_input[0] = '\0';
                            pass_stars[0] = '\0';
                            notify_push("Error", "Wrong password!");
                            dbg_log("Auth: Wrong Password");
                        }
                    } else if (code >= 0x02 && code <= 0x32 && pass_len < CRED_LEN - 1) {
                        const char *map = "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm";
                        char c = map[code - 0x02];
                        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                            pass_input[pass_len] = c;
                            pass_stars[pass_len] = '*';
                            pass_len++;
                            pass_input[pass_len] = '\0';
                            pass_stars[pass_len] = '\0';
                        }
                    }
                    continue;
                }

                if (k_alt && key_c && key_a && key_x) {
                    reg_win = 1;
                    dbg_log("Hotkey: CPU Registers");
                    asm volatile ("mov %%eax, %0; mov %%ebx, %1; mov %%ecx, %2; mov %%edx, %3; mov %%esp, %4; mov %%ebp, %5"
                        : "=m"(r_eax), "=m"(r_ebx), "=m"(r_ecx), "=m"(r_edx), "=m"(r_esp), "=m"(r_ebp));
                }

                if (k_win && k_ctrl && code == 0x30) { gui_settings_open(); dbg_log("Hotkey: Open Settings"); continue; }
                if (k_win && code == 0x20) { gui_term_open(); dbg_log("Hotkey: Open Terminal"); continue; }
                if (k_win && code == 0x13) { gui_files_open(); dbg_log("Hotkey: Open Files"); continue; }
                if (k_win && code == 0x1E) {
                    gui_term_close(); gui_files_close(); gui_settings_close();
                    gui_media_close(); gui_snake_close();
                    gui_3d_close(); gui_sappedit_close();
                    reg_win = 0;
                    dbg_log("Hotkey: Close All Windows");
                    continue;
                }

                if (k_ctrl && code == 0x2E) {
                    if (next_media_lba + 48 >= MAX_DISK_LBA) {
                        notify_push("Disk Full", "No space left!");
                        dbg_log("Screenshot: Disk Full");
                        continue;
                    }
                    char sname[NAME_LEN];
                    make_media_name(sname, "shot", shot_idx++, ".jpg");
                    capture_frame_to_disk(next_media_lba);
                    save_media_meta(sname, 1, next_media_lba, 1);
                    next_media_lba += 48;
                    notify_push("Screenshot", sname);
                    dbg_log("Saved Screenshot (.jpg)");
                    continue;
                }

                if (k_ctrl && code == 0x2F) {
                    if (!recording) {
                        if (next_media_lba + 48 >= MAX_DISK_LBA) {
                            notify_push("Disk Full", "No space left!");
                            dbg_log("ScreenRec: Disk Full");
                            continue;
                        }
                        recording = 1;
                        rec_frames = 0;
                        rec_div = 0;
                        rec_start_lba = next_media_lba;
                        notify_push("Screen Rec", "Recording...");
                        dbg_log("ScreenRec: Started");
                    } else {
                        recording = 0;
                        char rname[NAME_LEN];
                        make_media_name(rname, "rec", rec_idx++, ".mp4");
                        save_media_meta(rname, 2, rec_start_lba, rec_frames > 0 ? rec_frames : 1);
                        next_media_lba = rec_start_lba + (uint32_t)(rec_frames > 0 ? rec_frames : 1) * 48;
                        notify_push("Screen Rec", rname);
                        dbg_log("ScreenRec: Saved (.mp4)");
                    }
                    continue;
                }

                sc = code;
                dbg_log("Key: Scancode Input");
            }
        }

        get_rtc_time(time_str);

        if (is_locked) {
            int cx = g_scr_w / 2, cy = g_scr_h / 2;
            if (click && m.x >= cx - 120 && m.x <= cx + 120 && m.y >= cy + 96 && m.y <= cy + 142) {
                if (str_equal(pass_input, sys_pass)) {
                    is_locked = 0; pass_len = 0;
                    pass_input[0] = '\0'; pass_stars[0] = '\0';
                    notify_push("Sashix", sys_user);
                    dbg_log("Click: Login Success");
                } else {
                    pass_len = 0; pass_input[0] = '\0'; pass_stars[0] = '\0';
                    notify_push("Error", "Wrong password!");
                    dbg_log("Click: Wrong Password");
                }
            }
            draw_lock_screen(time_str);
            notify_draw();
            mouse_draw_cursor();
            screen_swap();
            continue;
        }

        if (rclick) {
            ctx_open = 1;
            ctx_x = m.x;
            ctx_y = m.y;
            if (ctx_x + 220 > g_scr_w) ctx_x = g_scr_w - 220;
            if (ctx_y + 200 > g_scr_h - 44) ctx_y = g_scr_h - 244;
            dbg_log("Mouse: Right-Click Context");
        }

        if (click) {
            int bar_y = g_scr_h - 42;
            if (ctx_open && m.x >= ctx_x && m.x <= ctx_x + 220 && m.y >= ctx_y && m.y <= ctx_y + 200) {
                int item = (m.y - ctx_y) / 40;
                ctx_open = 0;
                if (item == 0) { gui_term_copy_clip(); gui_sappedit_copy_clip(); dbg_log("Context: Copy"); }
                else if (item == 1) { gui_term_paste_clip(); gui_sappedit_paste_clip(); dbg_log("Context: Paste"); }
                else if (item == 2) { gui_term_open(); dbg_log("Context: Open Terminal"); }
                else if (item == 3) { gui_files_open(); dbg_log("Context: Open Files"); }
                else if (item == 4) { gui_sappedit_open(); dbg_log("Context: Open SappEdit"); }
            } else if (reg_win && m.x >= 685 && m.x <= 725 && m.y >= 154 && m.y <= 182) {
                reg_win = 0; ctx_open = 0;
                dbg_log("Click: Close Registers");
            } else if (m.x >= 8 && m.x <= 150 && m.y >= bar_y && m.y <= g_scr_h) {
                menu_open = !menu_open; ctx_open = 0;
                dbg_log(menu_open ? "Click: Open Start Menu" : "Click: Close Start Menu");
            } else if (menu_open && m.x >= 8 && m.x <= 248 && m.y >= bar_y - 420 && m.y <= bar_y) {
                menu_open = 0; ctx_open = 0;
                int my_rel = m.y - (bar_y - 420);
                if (my_rel < 60) { gui_sappedit_open(); dbg_log("Menu: Open SappEdit"); }
                else if (my_rel < 120) { gui_3d_open(); dbg_log("Menu: Open 3D Engine"); }
                else if (my_rel < 180) { gui_settings_open(); dbg_log("Menu: Open Settings"); }
                else if (my_rel < 240) { gui_files_open(); dbg_log("Menu: Open Files"); }
                else if (my_rel < 300) { gui_snake_open(); dbg_log("Menu: Open Snake"); }
                else if (my_rel < 360) { gui_term_open(); dbg_log("Menu: Open Terminal"); }
                else if (my_rel < 420) { dbg_log("Menu: Power Off"); sys_poweroff(); }
            } else {
                menu_open = 0;
                ctx_open = 0;
                if (m.x >= 32 && m.x <= 112 && m.y >= 32 && m.y <= 118) { gui_snake_open(); dbg_log("Icon: Open Snake"); }
                else if (m.x >= 32 && m.x <= 112 && m.y >= 140 && m.y <= 226) { gui_term_open(); dbg_log("Icon: Open Terminal"); }
                else if (m.x >= 32 && m.x <= 112 && m.y >= 248 && m.y <= 334) { gui_files_open(); dbg_log("Icon: Open Files"); }
                else if (m.x >= 32 && m.x <= 112 && m.y >= 356 && m.y <= 442) { gui_settings_open(); dbg_log("Icon: Open Settings"); }
                else if (m.x >= 32 && m.x <= 112 && m.y >= 464 && m.y <= 550) { gui_3d_open(); dbg_log("Icon: Open 3D Engine"); }
                else if (m.x >= 32 && m.x <= 112 && m.y >= 572 && m.y <= 658) { gui_sappedit_open(); dbg_log("Icon: Open SappEdit"); }
                else dbg_log("Mouse: Left Click");
            }
        }

        gui_media_update(m, click);
        gui_files_update(m, click);
        gui_settings_update(m, click);
        gui_snake_update(m, click, sc);
        gui_3d_update(m, click, sc);
        gui_sappedit_update(m, click, sc, k_ctrl, k_shift);
        gui_term_update(m, click, sc, k_ctrl, k_shift, k_alt);

        vfs_check_integrity();

        gui_settings_draw_bg();

        screen_round_rect(40, 32, 64, 64, 14, 0x1B5E20);
        screen_round_rect(50, 44, 14, 14, 4, 0x69F0AE);
        screen_round_rect(64, 44, 14, 14, 4, 0x69F0AE);
        screen_round_rect(64, 58, 14, 14, 4, 0x69F0AE);
        screen_round_rect(82, 58, 12, 12, 6, 0xFF5252);
        screen_str(24, 102, "Snake", 0xFFFFFF);

        screen_round_rect(40, 140, 64, 64, 14, 0x1C222D);
        screen_round_rect(44, 144, 56, 12, 4, 0x37474F);
        screen_str(48, 166, ">_", 0x00E676);
        screen_str(32, 210, "Term", 0xFFFFFF);

        screen_round_rect(40, 248, 64, 64, 14, 0xF57F17);
        screen_round_rect(40, 242, 30, 10, 4, 0xFFCA28);
        screen_str(24, 318, "Files", 0xFFFFFF);

        screen_round_rect(40, 356, 64, 64, 14, 0x455A64);
        screen_round_rect(54, 370, 36, 36, 18, 0xCFD8DC);
        screen_round_rect(64, 380, 16, 16, 8, 0x455A64);
        screen_str(16, 426, "Config", 0xFFFFFF);

        screen_round_rect(40, 464, 64, 64, 14, 0x1A237E);
        screen_round_rect(52, 476, 28, 28, 4, 0x00E5FF);
        screen_round_rect(62, 486, 28, 28, 4, 0x84FFFF);
        screen_str(48, 534, "3D", 0xFFFFFF);

        screen_round_rect(40, 572, 64, 64, 14, 0x4A148C);
        screen_str(52, 596, "<>", 0x00FFAA);
        screen_str(32, 642, "Sapp", 0xFFFFFF);

        gui_files_draw();
        gui_settings_draw();
        gui_term_draw();
        gui_media_draw();
        gui_snake_draw();
        gui_3d_draw();
        gui_sappedit_draw();

        if (reg_win) {
            screen_rect_alpha(280, 150, 450, 260, 0x1E1E2E, 230);
            screen_round_rect(280, 150, 450, 36, 8, 0x2B579A);
            screen_str(296, 160, "CPU REGISTERS", 0xFFFFFF);
            screen_round_rect(685, 154, 40, 28, 6, 0xAA2222);
            screen_str(697, 160, "X", 0xFFFFFF);

            to_hex(r_eax, hb); screen_str(300, 204, "EAX:", 0xAAAAAA); screen_str(380, 204, hb, 0x00FF00);
            to_hex(r_ebx, hb); screen_str(300, 234, "EBX:", 0xAAAAAA); screen_str(380, 234, hb, 0x00FF00);
            to_hex(r_ecx, hb); screen_str(300, 264, "ECX:", 0xAAAAAA); screen_str(380, 264, hb, 0x00FF00);
            to_hex(r_edx, hb); screen_str(300, 294, "EDX:", 0xAAAAAA); screen_str(380, 294, hb, 0x00FF00);
            to_hex(r_esp, hb); screen_str(300, 324, "ESP:", 0xAAAAAA); screen_str(380, 324, hb, 0xFFFF00);
            to_hex(r_ebp, hb); screen_str(300, 354, "EBP:", 0xAAAAAA); screen_str(380, 354, hb, 0xFFFF00);
        }

        int bar_y = g_scr_h - 42;
        screen_rect_alpha(0, bar_y, g_scr_w, 42, 0x141822, 200);
        screen_round_rect(6, bar_y + 3, 140, 36, 8, menu_open ? 0x0078D7 : 0x2B579A);
        screen_str(24, bar_y + 13, "Sashix", 0xFFFFFF);

        if (recording) {
            screen_round_rect(164, bar_y + 11, 20, 20, 10, 0xFF1744);
            screen_str(192, bar_y + 13, "REC", 0xFF5252);
            rec_div++;
            if (rec_div >= 6) {
                rec_div = 0;
                uint32_t target_lba = rec_start_lba + (uint32_t)rec_frames * 48;
                if (target_lba + 48 < MAX_DISK_LBA) {
                    capture_frame_to_disk(target_lba);
                    rec_frames++;
                } else {
                    recording = 0;
                    char rname[NAME_LEN];
                    make_media_name(rname, "rec", rec_idx++, ".mp4");
                    save_media_meta(rname, 2, rec_start_lba, rec_frames);
                    next_media_lba = target_lba;
                    notify_push("Disk Full", "Rec saved!");
                    dbg_log("ScreenRec: Disk Full Auto-Save");
                }
            }
        }

        if (dev_mode == 1) {
            fps_count++;
            uint8_t cur_sec = read_rtc(0x00);
            if (cur_sec != last_sec) {
                fps_val = fps_count;
                fps_count = 0;
                last_sec = cur_sec;
                make_fps_str(fps_val, fps_str);
            }
            int fx = recording ? 265 : 168;
            screen_round_rect(fx, bar_y + 5, 148, 32, 6, 0x1B3B2B);
            screen_str(fx + 10, bar_y + 13, fps_str, 0x00FF88);
        } else if (dev_mode == 2) {
            int dx = recording ? 265 : 168;
            int dw = g_scr_w - dx - 185;
            if (dw > 120) {
                screen_round_rect(dx, bar_y + 5, dw, 32, 6, 0x2B2118);
                screen_str(dx + 10, bar_y + 13, dbg_msg, 0xFFCC44);
            }
        }

        screen_str(g_scr_w - 170, bar_y + 13, g_kbd_lang ? "RUS" : "ENG", 0x00CCFF);
        screen_str(g_scr_w - 96, bar_y + 13, time_str, 0xFFFFFF);

        if (menu_open) {
            screen_rect_alpha(8, bar_y - 420, 240, 420, 0x1A1F2C, 225);
            screen_str(24, bar_y - 400, "Sapp Edit", 0xCE93D8);
            screen_str(24, bar_y - 340, "3D Engine", 0x00E5FF);
            screen_str(24, bar_y - 280, "Settings", 0xFFFFFF);
            screen_str(24, bar_y - 220, "Files", 0xFFFFFF);
            screen_str(24, bar_y - 160, "Snake", 0xFFFFFF);
            screen_str(24, bar_y - 100, "Terminal", 0xFFFFFF);
            screen_str(24, bar_y - 40,  "Power Off", 0xFF5252);
        }

        if (ctx_open) {
            screen_rect_alpha(ctx_x, ctx_y, 220, 200, 0x1E2430, 235);
            screen_str(ctx_x + 14, ctx_y + 12,  "Copy (Ctrl+A)", 0x00FFAA);
            screen_str(ctx_x + 14, ctx_y + 52,  "Paste (Ctrl+S)", 0x00CCFF);
            screen_str(ctx_x + 14, ctx_y + 92,  "Terminal", 0xFFFFFF);
            screen_str(ctx_x + 14, ctx_y + 132, "Files", 0xFFFFFF);
            screen_str(ctx_x + 14, ctx_y + 172, "Sapp Edit", 0xCE93D8);
        }

        notify_draw();
        mouse_draw_cursor();
        screen_swap();
    }
}