#include "Drivers/disk/types.h"
#include "Drivers/disk/io.h"
#include "Drivers/screen/screen.h"
#include "Drivers/screen/mouse.h"
#include "Drivers/keyboard/keyboard.h"

extern void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer);
extern void ata_write_sector_drv(uint8_t drive, uint32_t lba, const uint16_t *source_buffer);
extern int  ata_identify_drive(uint8_t drive, char *model_out);
extern uint8_t g_sys_lang;
extern uint8_t g_kbd_lang;

#define SEC_WORDS 256
#define CRED_LEN  24
#define FS_LBA    150

uint8_t g_mounted_fs = 0;
static uint16_t mbr_buf[SEC_WORDS];
static uint16_t fmt_sec[SEC_WORDS];

static void format_and_mount_disk(uint8_t fs_type, uint8_t lang) {
    int cx = g_scr_w / 2;
    int cy = g_scr_h / 2;

    for (int i = 0; i < SEC_WORDS; i++) fmt_sec[i] = 0;

    for (int s = 0; s <= 16; s++) {
        ata_write_sector_drv(0, FS_LBA + (uint32_t)s, fmt_sec);

        screen_clear(0x0B0F17);
        screen_round_rect(cx - 250, cy - 140, 500, 280, 14, 0x151B26);
        screen_round_rect(cx - 250, cy - 140, 500, 46, 14, 0x2B579A);
        screen_str(cx - 180, cy - 125, lang ? "ФОРМАТИРОВАНИЕ..." : "FORMATTING DISK...", 0xFFFFFF);

        screen_str(cx - 200, cy - 40,
                   fs_type == 0 ? "FS: SashixFS v2 (LBA 150)" : "FS: FAT32 Compat (LBA 150)",
                   0x00FFAA);

        screen_round_rect(cx - 200, cy + 10, 400, 32, 8, 0x0D1117);
        int bar_w = ((s + 1) * 392) / 17;
        screen_round_rect(cx - 196, cy + 14, bar_w, 24, 6, 0x238636);

        screen_str(cx - 140, cy + 68, lang ? "Монтирование раздела..." : "Mounting partition...", 0x8B949E);
        screen_swap();

        for (volatile uint32_t d = 0; d < 800000; d++) asm volatile ("nop");
    }

    g_mounted_fs = fs_type;
}

void installer_init(void) {
    screen_init();
    ata_read_sector_drv(0, 0, mbr_buf);
    uint8_t *r = (uint8_t *)mbr_buf;

    if (r[490] == 'S' && r[491] == 'A' && r[492] == 'S' && r[493] == 'H') {
        g_sys_lang = r[494];
        g_mounted_fs = r[489];
        return;
    }

    mouse_init();
    keyboard_init();

    char disk_name[48];
    if (!ata_identify_drive(0, disk_name)) {
        const char *def = "QEMU HARDDISK (ATA0)";
        int k = 0;
        while (def[k]) { disk_name[k] = def[k]; k++; }
        disk_name[k] = '\0';
    }
    disk_name[26] = '\0';

    int step = 0;
    uint8_t lang = 1;
    uint8_t fs_type = 0;
    int fs_formatted = 0;

    char user[CRED_LEN] = "sasha";
    int ulen = 5;
    int uchars = 5;
    char pass[CRED_LEN] = "";
    char stars[CRED_LEN] = "";
    int plen = 0;
    int pchars = 0;
    int active_field = 0;
    uint8_t prev_click = 0;
    uint8_t ext_code = 0;
    uint8_t k_shift = 0;

    while (1) {
        mouse_poll();
        mouse_state_t m = mouse_get_state();
        uint8_t click = (m.left_btn && !prev_click);
        prev_click = m.left_btn;

        int cx = g_scr_w / 2;
        int cy = g_scr_h / 2;

        while (inb(0x64) & 1) {
            uint8_t st = inb(0x64);
            if (st & 0x20) break;
            uint8_t raw = inb(0x60);

            if (raw == 0xE0) { ext_code = 1; continue; }
            uint8_t released = (raw & 0x80) ? 1 : 0;
            uint8_t code = raw & 0x7F;
            if (ext_code) { ext_code = 0; continue; }

            if (code == 0x2A || code == 0x36) {
                k_shift = !released;
                continue;
            }
            if (released) continue;

            if (code == 0x40) {
                keyboard_toggle_lang();
                continue;
            }

            if (step == 2) {
                if (code == 0x0F || code == 0x1C) {
                    active_field = !active_field;
                    continue;
                }
                if (code == 0x0E) {
                    if (active_field == 0 && ulen > 0) {
                        ulen--;
                        if (ulen > 0 && ((uint8_t)user[ulen] & 0xC0) == 0x80) ulen--;
                        user[ulen] = '\0';
                        if (uchars > 0) uchars--;
                    } else if (active_field == 1 && plen > 0) {
                        plen--;
                        if (plen > 0 && ((uint8_t)pass[plen] & 0xC0) == 0x80) plen--;
                        pass[plen] = '\0';
                        if (pchars > 0) pchars--;
                        stars[pchars] = '\0';
                    }
                } else {
                    const char *ch_str = keyboard_scancode_to_utf8(code, k_shift);
                    if (ch_str != '\0' && ch_str != '\n' && ch_str != '\b') {
                        int k = 0;
                        if (active_field == 0 && uchars < 10 && ulen < CRED_LEN - 3) {
                            while (ch_str[k]) {
                                user[ulen++] = ch_str[k++];
                            }
                            user[ulen] = '\0';
                            uchars++;
                        } else if (active_field == 1 && pchars < 7 && plen < CRED_LEN - 3) {
                            while (ch_str[k]) {
                                pass[plen++] = ch_str[k++];
                            }
                            pass[plen] = '\0';
                            stars[pchars++] = '*';
                            stars[pchars] = '\0';
                        }
                    }
                }
            }
        }

        if (click) {
            if (step == 0) {
                if (m.x >= cx - 190 && m.x <= cx - 10 && m.y >= cy - 20 && m.y <= cy + 30) { lang = 1; g_kbd_lang = 1; }
                if (m.x >= cx + 10 && m.x <= cx + 190 && m.y >= cy - 20 && m.y <= cy + 30) { lang = 0; g_kbd_lang = 0; }
                if (m.x >= cx - 120 && m.x <= cx + 120 && m.y >= cy + 80 && m.y <= cy + 126) step = 1;
            } else if (step == 1) {
                if (m.x >= cx - 210 && m.x <= cx - 10 && m.y >= cy - 10 && m.y <= cy + 44) {
                    fs_type = 0;
                    fs_formatted = 0;
                }
                if (m.x >= cx + 10 && m.x <= cx + 210 && m.y >= cy - 10 && m.y <= cy + 44) {
                    fs_type = 1;
                    fs_formatted = 0;
                }
                if (m.x >= cx - 190 && m.x <= cx + 190 && m.y >= cy + 64 && m.y <= cy + 108) {
                    format_and_mount_disk(fs_type, lang);
                    fs_formatted = 1;
                }
                if (fs_formatted && m.x >= cx - 120 && m.x <= cx + 120 && m.y >= cy + 124 && m.y <= cy + 168) {
                    step = 2;
                }
            } else if (step == 2) {
                if (m.x >= cx - 180 && m.x <= cx + 180 && m.y >= cy - 50 && m.y <= cy - 6) active_field = 0;
                if (m.x >= cx - 180 && m.x <= cx + 180 && m.y >= cy + 30 && m.y <= cy + 74) active_field = 1;
                if (m.x >= cx - 130 && m.x <= cx + 130 && m.y >= cy + 105 && m.y <= cy + 150) {
                    g_sys_lang = lang;

                    r[446] = 0x80;
                    r[450] = (fs_type == 0) ? 0x83 : 0x0B;
                    *(uint32_t *)(r + 454) = FS_LBA;
                    *(uint32_t *)(r + 458) = 20000;

                    r[489] = fs_type;
                    r[490] = 'S'; r[491] = 'A'; r[492] = 'S'; r[493] = 'H';
                    r[494] = lang;
                    for (int i = 0; i < 8; i++) {
                        r[495 + i] = (i < ulen) ? (uint8_t)user[i] : 0;
                        r[503 + i] = (i < plen) ? (uint8_t)pass[i] : 0;
                    }
                    r[510] = 0x55;
                    r[511] = 0xAA;
                    ata_write_sector_drv(0, 0, mbr_buf);
                    return;
                }
            }
        }

        screen_clear(0x0B0F17);
        screen_round_rect(cx - 260, cy - 210, 520, 420, 14, 0x151B26);
        screen_round_rect(cx - 260, cy - 210, 520, 48, 14, 0x2B579A);

        if (step == 0) {
            screen_round_rect(cx - 32, cy - 140, 64, 64, 12, 0x1E2532);
            screen_str(cx - 16, cy - 116, ">_", 0x00FF88);

            screen_str(cx - 144, cy - 194, "SASHIX SETUP", 0xFFFFFF);
            screen_str(cx - 170, cy - 54, lang ? "Выберите язык:" : "Select language:", 0xCCCCCC);

            screen_round_rect(cx - 190, cy - 20, 180, 50, 8, lang == 1 ? 0x0078D7 : 0x222A38);
            screen_str(cx - 145, cy - 3, "Русский", 0xFFFFFF);

            screen_round_rect(cx + 10, cy - 20, 180, 50, 8, lang == 0 ? 0x0078D7 : 0x222A38);
            screen_str(cx + 45, cy - 3, "English", 0xFFFFFF);

            screen_round_rect(cx - 120, cy + 80, 240, 46, 8, 0x238636);
            screen_str(cx - 56, cy + 95, lang ? "ДАЛЕЕ" : "NEXT", 0xFFFFFF);
        } else if (step == 1) {
            screen_str(cx - 190, cy - 194, lang ? "МОНТИРОВАНИЕ ДИСКА" : "DISK & FILESYSTEM", 0xFFFFFF);

            screen_round_rect(cx - 210, cy - 140, 420, 68, 8, 0x0D1117);
            screen_str(cx - 194, cy - 130, lang ? "Накопитель ATA0:" : "Target Drive ATA0:", 0x8B949E);
            screen_str(cx - 194, cy - 102, disk_name, 0x58A6FF);

            screen_str(cx - 210, cy - 44, lang ? "Файловая система:" : "Choose Filesystem:", 0xCCCCCC);

            screen_round_rect(cx - 210, cy - 10, 200, 54, 8, fs_type == 0 ? 0x0078D7 : 0x222A38);
            screen_str(cx - 174, cy + 8, "SashixFS", 0xFFFFFF);

            screen_round_rect(cx + 10, cy - 10, 200, 54, 8, fs_type == 1 ? 0x0078D7 : 0x222A38);
            screen_str(cx + 68, cy + 8, "FAT32", 0xFFFFFF);

            screen_round_rect(cx - 190, cy + 64, 380, 44, 8, fs_formatted ? 0x1B5E20 : 0xD97706);
            if (fs_formatted) {
                screen_str(cx - 150, cy + 78, lang ? "Смонтировано (/)" : "Mounted at (/)", 0x69F0AE);
            } else {
                screen_str(cx - 166, cy + 78, lang ? "Форматировать диск" : "Format & Mount", 0xFFFFFF);
            }

            if (fs_formatted) {
                screen_round_rect(cx - 120, cy + 124, 240, 44, 8, 0x238636);
                screen_str(cx - 56, cy + 138, lang ? "ДАЛЕЕ" : "NEXT", 0xFFFFFF);
            }
        } else {
            screen_str(cx - 176, cy - 194, lang ? "СОЗДАНИЕ ПРОФИЛЯ" : "CREATE PROFILE", 0xFFFFFF);

            screen_str(cx - 180, cy - 74, lang ? "Имя пользователя:" : "Username:", 0xAAAAAA);
            screen_round_rect(cx - 180, cy - 50, 360, 44, 8, active_field == 0 ? 0x263244 : 0x0D1117);
            screen_str(cx - 166, cy - 36, user, 0x58A6FF);
            if (active_field == 0) screen_str(cx - 166 + uchars * 16, cy - 36, "_", 0xFFFFFF);

            screen_str(cx - 180, cy + 6, lang ? "Пароль (необяз.):" : "Password (opt):", 0xAAAAAA);
            screen_round_rect(cx - 180, cy + 30, 360, 44, 8, active_field == 1 ? 0x263244 : 0x0D1117);
            screen_str(cx - 166, cy + 44, stars, 0x00FF88);
            if (active_field == 1) screen_str(cx - 166 + pchars * 16, cy + 44, "_", 0xFFFFFF);

            screen_round_rect(cx - 130, cy + 105, 260, 46, 8, 0x238636);
            screen_str(cx - 88, cy + 120, lang ? "УСТАНОВИТЬ" : "INSTALL", 0xFFFFFF);

            screen_str(cx - 144, cy + 168, g_kbd_lang ? "Раскладка: РУС (F6)" : "Layout: ENG (F6)", 0x8B949E);
        }

        mouse_draw_cursor();
        screen_swap();
    }
}