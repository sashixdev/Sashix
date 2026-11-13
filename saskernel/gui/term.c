#include "apps.h"
#include "../vfs.h"
#include "../Drivers/disk/io.h"
#include "../Drivers/screen/screen.h"
#include "../Drivers/screen/notify.h"
#include "../Drivers/keyboard/keyboard.h"

extern void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer);
extern int ata_identify_drive(uint8_t drive, char *model_out);
extern uint8_t g_sys_lang;
extern char g_clipboard[];

#define TERM_LINES   24
#define LINE_LEN     52
#define CMD_MAX      44
#define NSA_BUF_MAX  1024
#define DIR_MAX      16
#define FILE_MAX     24
#define MBR_WORDS    256
#define PASS_MAX     8
#define MAT_COLS     48

static int is_open = 0;
static int is_max = 0;
static int tx = 140, ty = 70;
static int dragging = 0, dox = 0, doy = 0;

static char cur_dir[DIR_MAX] = "/Users";
static char lines[TERM_LINES][LINE_LEN];
static char cmd[CMD_MAX];
static int cmd_len = 0;

static int nsa_mode = 0;
static char nsa_file[FILE_MAX];
static char nsa_buf[NSA_BUF_MAX];
static int nsa_len = 0;

static int matrix_mode = 0;
static int mat_y[MAT_COLS];
static uint32_t mat_seed = 12345;

static void t_cpy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int t_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int t_starts(const char *s, const char *pref) {
    int i = 0;
    while (pref[i]) {
        if (s[i] != pref[i]) return 0;
        i++;
    }
    return 1;
}

static void term_print(const char *s) {
    for (int i = 0; i < TERM_LINES - 1; i++) {
        t_cpy(lines[i], lines[i + 1], LINE_LEN);
    }
    t_cpy(lines[TERM_LINES - 1], s, LINE_LEN);
}

static void speak_text(const char *txt) {
    if (g_settings.volume == 0) return;
    if (!txt || txt[0] == '\0') txt = "sashix";

    for (int i = 0; txt[i]; i++) {
        uint8_t ch = (uint8_t)txt[i];
        if (ch <= 32) {
            for (volatile uint32_t d = 0; d < 12000000; d++) asm volatile ("nop");
            continue;
        }
        uint32_t freq = 350 + (ch % 32) * 28;
        uint32_t div = 1193180 / freq;

        outb(0x43, 0xB6);
        outb(0x42, (uint8_t)(div & 0xFF));
        outb(0x42, (uint8_t)((div >> 8) & 0xFF));

        uint8_t tmp = inb(0x61);
        outb(0x61, tmp | 3);

        for (volatile uint32_t d = 0; d < 15000000; d++) asm volatile ("nop");

        outb(0x61, inb(0x61) & 0xFC);

        for (volatile uint32_t d = 0; d < 3000000; d++) asm volatile ("nop");
    }
}

static void int_to_str(uint32_t val, char *out) {
    if (val == 0) {
        out[0] = '0'; out[1] = '\0'; return;
    }
    char tmp[12];
    int p = 0;
    while (val > 0 && p < 11) {
        tmp[p++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = 0; i < p; i++) out[i] = tmp[p - 1 - i];
    out[p] = '\0';
}

static void print_real_sysinfo(void) {
    uint32_t eax, ebx, ecx, edx;
    char vendor[16];
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *(uint32_t *)(vendor + 0) = ebx;
    *(uint32_t *)(vendor + 4) = edx;
    *(uint32_t *)(vendor + 8) = ecx;
    vendor[12] = '\0';

    char brand[52];
    brand[0] = '\0';
    asm volatile ("cpuid" : "=a"(eax) : "a"(0x80000000) : "ebx", "ecx", "edx");
    if (eax >= 0x80000004) {
        for (uint32_t i = 0; i < 3; i++) {
            asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002 + i));
            *(uint32_t *)(brand + i * 16 + 0)  = eax;
            *(uint32_t *)(brand + i * 16 + 4)  = ebx;
            *(uint32_t *)(brand + i * 16 + 8)  = ecx;
            *(uint32_t *)(brand + i * 16 + 12) = edx;
        }
        brand[48] = '\0';
    }

    outb(0x70, 0x30); uint8_t low  = inb(0x71);
    outb(0x70, 0x31); uint8_t high = inb(0x71);
    uint32_t ext_kb = ((uint32_t)high << 8) | low;
    outb(0x70, 0x34); uint8_t b34 = inb(0x71);
    outb(0x70, 0x35); uint8_t b35 = inb(0x71);
    uint32_t above16_64k = ((uint32_t)b35 << 8) | b34;
    uint32_t ram_mb = (above16_64k > 0) ? (16 + (above16_64k * 64) / 1024) : (1 + ext_kb / 1024);

    char ram_line[LINE_LEN];
    char num[12];
    int_to_str(ram_mb, num);
    t_cpy(ram_line, "RAM (CMOS): ", LINE_LEN);
    int p = 12;
    for (int i = 0; num[i] && p < LINE_LEN - 4; i++) ram_line[p++] = num[i];
    ram_line[p++] = ' '; ram_line[p++] = 'M'; ram_line[p++] = 'B'; ram_line[p] = '\0';

    char cpu_line[LINE_LEN];
    t_cpy(cpu_line, "CPU Vendor: ", LINE_LEN);
    p = 12;
    for (int i = 0; vendor[i] && p < LINE_LEN - 1; i++) cpu_line[p++] = vendor[i];
    cpu_line[p] = '\0';

    term_print(cpu_line);
    if (brand[0] != '\0') {
        const char *b_trim = brand;
        while (*b_trim == ' ') b_trim++;
        term_print(b_trim);
    }
    term_print(ram_line);
    term_print(vfs_is_root() ? "Privilege: ROOT (Ring 0)" : "Privilege: USER");
}

static void print_real_hostinfo(void) {
    const char *board = "QEMU / x86 PC";
    uint8_t *mem = (uint8_t *)0x000F0000;
    for (uint32_t off = 0; off < 0xFFF0; off += 16) {
        if (mem[off + 0] == '_' && mem[off + 1] == 'S' &&
            mem[off + 2] == 'M' && mem[off + 3] == '_') {
            uint32_t tbl_addr = *(uint32_t *)(mem + off + 0x18);
            uint16_t tbl_len  = *(uint16_t *)(mem + off + 0x16);
            if (tbl_addr > 0x10000 && tbl_addr < 0x100000) {
                uint8_t *p = (uint8_t *)tbl_addr;
                uint8_t *end = p + tbl_len;
                while (p + 4 < end) {
                    uint8_t type = p[0];
                    uint8_t len  = p[1];
                    if (len < 4) break;
                    if (type == 2 || type == 1) {
                        uint8_t str_idx = p[5];
                        char *s = (char *)(p + len);
                        uint8_t cur = 1;
                        while (*s && cur < str_idx) {
                            while (*s) s++;
                            s++;
                            cur++;
                        }
                        if (cur == str_idx && *s) { board = s; if (type == 2) break; }
                    }
                    p += len;
                    while (p + 1 < end && !(p[0] == 0 && p[1] == 0)) p++;
                    p += 2;
                }
            }
            break;
        }
    }

    char hdd[48];
    if (!ata_identify_drive(0, hdd)) t_cpy(hdd, "No IDE Master", 48);

    term_print("Board (SMBIOS):");
    term_print(board);
    term_print("Storage (ATA0):");
    term_print(hdd);
}

static int check_root_pass(const char *entered) {
    uint16_t mbr[MBR_WORDS];
    ata_read_sector_drv(0, 0, mbr);
    uint8_t *r = (uint8_t *)mbr;
    if (r[490] != 'S' || r[491] != 'A') return 1;
    char saved[PASS_MAX];
    int i = 0;
    while (r[503 + i] && i < PASS_MAX - 1) {
        saved[i] = (char)r[503 + i];
        i++;
    }
    saved[i] = '\0';
    if (i == 0) return 1;
    return t_eq(entered, saved);
}

static void exec_command(void) {
    term_print(cmd);
    if (cmd_len == 0) return;

    if (t_eq(cmd, "help")) {
        term_print("dirls, cd, cr dir/file, cat, rm, nsa");
        term_print("echo, speak, sysinfo, hostinfo, sasfast");
        term_print("matrix, su, panic, power, rboot, clear");
    } else if (t_eq(cmd, "power")) {
        asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
        asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
        asm volatile ("cli; hlt");
    } else if (t_eq(cmd, "rboot") || t_eq(cmd, "reboot")) {
        while (inb(0x64) & 0x02);
        outb(0x64, 0xFE);
        asm volatile ("hlt");
    } else if (t_eq(cmd, "clear")) {
        for (int i = 0; i < TERM_LINES; i++) lines[i][0] = '\0';
    } else if (t_eq(cmd, "sasfast")) {
        term_print("  \\ \\              Sashix 32-bit");
        term_print("   \\ \\             Kernel: sasKernel x86");
        term_print("    > >            FS: SashixFS V2");
        term_print("   / /             GUI: VESA FullScreen");
        term_print("  /_/  ______      Shell: SashixTerm");
    } else if (t_eq(cmd, "sysinfo")) {
        print_real_sysinfo();
    } else if (t_eq(cmd, "hostinfo")) {
        print_real_hostinfo();
    } else if (t_starts(cmd, "echo ")) {
        term_print(cmd + 5);
    } else if (t_eq(cmd, "speak")) {
        term_print("Speaking...");
        speak_text("sashix");
    } else if (t_starts(cmd, "speak ")) {
        term_print(cmd + 6);
        speak_text(cmd + 6);
    } else if (t_eq(cmd, "matrix")) {
        for (int i = 0; i < MAT_COLS; i++) mat_y[i] = (i * 19) % 400;
        matrix_mode = 1;
    } else if (t_eq(cmd, "panic")) {
        asm volatile ("int $3");
    } else if (t_eq(cmd, "dirls")) {
        if (t_eq(cur_dir, "/")) {
            term_print("[DIR] /Sashix");
            term_print("[DIR] /Users");
            term_print("[DIR] /Apps");
            term_print("[DIR] /Media");
            return;
        }
        int found = 0;
        for (int i = 0; i < vfs_count(); i++) {
            vfs_node_t *n = vfs_get_node(i);
            if (n && t_eq(n->dir, cur_dir)) {
                term_print(n->name);
                found++;
            }
        }
        if (!found) term_print("(empty)");
    } else if (t_starts(cmd, "cd ")) {
        const char *arg = cmd + 3;
        if (t_eq(arg, "..") || t_eq(arg, "/")) t_cpy(cur_dir, "/", DIR_MAX);
        else if (t_eq(arg, "/Sashix") || t_eq(arg, "Sashix")) t_cpy(cur_dir, "/Sashix", DIR_MAX);
        else if (t_eq(arg, "/Users") || t_eq(arg, "Users")) t_cpy(cur_dir, "/Users", DIR_MAX);
        else if (t_eq(arg, "/Apps") || t_eq(arg, "Apps")) t_cpy(cur_dir, "/Apps", DIR_MAX);
        else if (t_eq(arg, "/Media") || t_eq(arg, "Media")) t_cpy(cur_dir, "/Media", DIR_MAX);
        else term_print("Dirs: /Sashix /Users /Apps /Media");
    } else if (t_starts(cmd, "cr dir ")) {
        const char *dname = cmd + 7;
        if (*dname) {
            vfs_write_file(cur_dir, dname, VFS_TYPE_FILE, 0, "[DIR]", 5);
            term_print("Directory created.");
        }
    } else if (t_starts(cmd, "cr file ")) {
        const char *fname = cmd + 8;
        if (*fname) {
            vfs_write_file(cur_dir, fname, VFS_TYPE_FILE, 0, "", 0);
            term_print("File created.");
        }
    } else if (t_starts(cmd, "cat ")) {
        vfs_node_t *n = vfs_find(cur_dir, cmd + 4);
        if (!n) {
            term_print("File not found.");
        } else {
            char row[LINE_LEN];
            int p = 0;
            for (uint32_t i = 0; i < n->size; i++) {
                char c = (char)n->data[i];
                if (c == '\n' || p >= LINE_LEN - 2) {
                    row[p] = '\0';
                    term_print(row);
                    p = 0;
                } else {
                    row[p++] = c;
                }
            }
            if (p > 0) { row[p] = '\0'; term_print(row); }
        }
    } else if (t_starts(cmd, "rm ")) {
        int res = vfs_delete_file(cur_dir, cmd + 3);
        if (res == 0) term_print("Removed.");
        else if (res == -1) term_print("Denied: ROOT required.");
        else term_print("Not found.");
    } else if (t_starts(cmd, "su")) {
        const char *pw = (cmd_len > 3) ? (cmd + 3) : "";
        if (check_root_pass(pw)) {
            vfs_set_root(1);
            term_print("ROOT mode enabled.");
            notify_push("Security", "ROOT mode active");
        } else {
            term_print("Wrong pass. Use: su <pass>");
        }
    } else if (t_starts(cmd, "nsa ")) {
        const char *fname = cmd + 4;
        if (!*fname) { term_print("Usage: nsa <file>"); return; }
        t_cpy(nsa_file, fname, FILE_MAX);
        nsa_len = 0;
        nsa_buf[0] = '\0';
        vfs_node_t *ex = vfs_find(cur_dir, fname);
        if (ex) {
            for (uint32_t i = 0; i < ex->size && i < NSA_BUF_MAX - 1; i++) {
                nsa_buf[i] = (char)ex->data[i];
                nsa_len++;
            }
            nsa_buf[nsa_len] = '\0';
        }
        nsa_mode = 1;
    } else {
        term_print("Unknown command. Type 'help'");
    }
}

void gui_term_init(void) {
    is_open = 0; is_max = 0; nsa_mode = 0; matrix_mode = 0;
    cmd_len = 0; cmd[0] = '\0';
    for (int i = 0; i < TERM_LINES; i++) lines[i][0] = '\0';
    term_print("Sashix Shell (Type 'help')");
}

void gui_term_open(void)  { is_open = 1; }
void gui_term_close(void) { is_open = 0; nsa_mode = 0; matrix_mode = 0; }

void gui_term_copy_clip(void) {
    if (!is_open) return;
    if (nsa_mode) t_cpy(g_clipboard, nsa_buf, 1024);
    else t_cpy(g_clipboard, cmd, 1024);
    notify_push("Clipboard", "Copied (Ctrl+A)");
}

void gui_term_paste_clip(void) {
    if (!is_open) return;
    int i = 0;
    if (nsa_mode) {
        while (g_clipboard[i] && nsa_len < NSA_BUF_MAX - 1) {
            nsa_buf[nsa_len++] = g_clipboard[i++];
        }
        nsa_buf[nsa_len] = '\0';
    } else {
        while (g_clipboard[i] && g_clipboard[i] != '\n' && cmd_len < CMD_MAX - 1) {
            cmd[cmd_len++] = g_clipboard[i++];
        }
        cmd[cmd_len] = '\0';
    }
}

void gui_term_update(mouse_state_t m, uint8_t click, uint8_t sc, uint8_t ctrl, uint8_t shift, uint8_t alt) {
    (void)alt;
    if (!is_open) return;

    int wx = is_max ? 0 : tx;
    int wy = is_max ? 0 : ty;
    int ww = is_max ? g_scr_w : 820;

    if (click) {
        if (m.x >= wx + ww - 44 && m.x <= wx + ww - 8 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_open = 0; nsa_mode = 0; matrix_mode = 0; return;
        } else if (m.x >= wx + ww - 86 && m.x <= wx + ww - 50 && m.y >= wy + 4 && m.y <= wy + 34) {
            is_max = !is_max; return;
        } else if (!is_max && m.x >= wx && m.x <= wx + ww && m.y >= wy && m.y <= wy + 38) {
            dragging = 1; dox = m.x - tx; doy = m.y - ty;
        }
    }

    if (!m.left_btn) dragging = 0;
    else if (dragging && !is_max) {
        tx = m.x - dox; ty = m.y - doy;
        if (tx < 0) tx = 0;
        if (tx > g_scr_w - 820) tx = g_scr_w - 820;
        if (ty < 0) ty = 0;
        if (ty > g_scr_h - 540) ty = g_scr_h - 540;
    }

    if (!sc) return;

    if (ctrl && sc == 0x1E) { gui_term_copy_clip(); return; }
    if (ctrl && sc == 0x1F) { gui_term_paste_clip(); return; }

    if (matrix_mode) {
        if (sc == 0x01 || sc == 0x1C) matrix_mode = 0;
        return;
    }

    if (nsa_mode) {
        if (sc == 0x01) {
            vfs_write_file(cur_dir, nsa_file, VFS_TYPE_FILE, 0, nsa_buf, (uint32_t)nsa_len);
            nsa_mode = 0;
            term_print("Saved (Esc).");
            notify_push("NSA Editor", nsa_file);
        } else if (sc == 0x1C && nsa_len < NSA_BUF_MAX - 1) {
            nsa_buf[nsa_len++] = '\n';
            nsa_buf[nsa_len] = '\0';
        } else if (sc == 0x0E && nsa_len > 0) {
            nsa_len--;
            if (nsa_len > 0 && ((uint8_t)nsa_buf[nsa_len] & 0xC0) == 0x80) nsa_len--;
            nsa_buf[nsa_len] = '\0';
        } else {
            const char *utf = keyboard_scancode_to_utf8(sc, shift);
            int k = 0;
            while (utf[k] && nsa_len < NSA_BUF_MAX - 1) {
                nsa_buf[nsa_len++] = utf[k++];
            }
            nsa_buf[nsa_len] = '\0';
        }
        return;
    }

    if (sc == 0x0E && cmd_len > 0) {
        cmd_len--;
        if (cmd_len > 0 && ((uint8_t)cmd[cmd_len] & 0xC0) == 0x80) cmd_len--;
        cmd[cmd_len] = '\0';
    } else if (sc == 0x1C) {
        cmd[cmd_len] = '\0';
        exec_command();
        cmd_len = 0;
        cmd[0] = '\0';
    } else {
        const char *utf = keyboard_scancode_to_utf8(sc, shift);
        int k = 0;
        while (utf[k] && cmd_len < CMD_MAX - 1) {
            cmd[cmd_len++] = utf[k++];
        }
        cmd[cmd_len] = '\0';
    }
}

void gui_term_draw(void) {
    if (!is_open) return;

    int wx = is_max ? 0 : tx;
    int wy = is_max ? 0 : ty;
    int ww = is_max ? g_scr_w : 820;
    int wh = is_max ? (g_scr_h - 44) : 500;

    screen_rect_alpha(wx, wy, ww, wh, 0x0E121A, 195);
    screen_round_rect(wx, wy, ww, 38, 8, 0x1E2532);

    screen_str(wx + 16, wy + 11, nsa_mode ? "NSA (Enter: NewLine | Esc: Save)" : "Sashix Terminal", 0xFFFFFF);

    screen_round_rect(wx + ww - 86, wy + 5, 34, 28, 6, 0x2E7D32);
    screen_str(wx + ww - 76, wy + 11, is_max ? "v" : "^", 0xFFFFFF);

    screen_round_rect(wx + ww - 44, wy + 5, 34, 28, 6, 0xC62828);
    screen_str(wx + ww - 34, wy + 11, "X", 0xFFFFFF);

    if (matrix_mode) {
        for (int c = 0; c < MAT_COLS; c++) {
            int cx = wx + 16 + c * 16;
            if (cx + 16 >= wx + ww - 8) break;
            mat_seed = mat_seed * 1103515245 + 12345;
            char ch = '0' + ((mat_seed >> 16) % 42);
            int cy = wy + 44 + (mat_y[c] % (wh - 64));
            screen_char(cx, cy, ch, 0x00FF44);
            mat_y[c] += 12;
        }
        return;
    }

    if (nsa_mode) {
        int rx = wx + 16, ry = wy + 48;
        char line_buf[LINE_LEN];
        int lp = 0;
        for (int i = 0; i <= nsa_len; i++) {
            char c = nsa_buf[i];
            if (c == '\n' || c == '\0' || lp >= LINE_LEN - 2) {
                line_buf[lp] = '\0';
                if (ry + 20 < wy + wh - 10) {
                    screen_str(rx, ry, line_buf, 0x00FFAA);
                    ry += 22;
                }
                lp = 0;
                if (c == '\0') break;
            } else {
                line_buf[lp++] = c;
            }
        }
        return;
    }

    int max_vis = (wh - 80) / 18;
    if (max_vis > TERM_LINES) max_vis = TERM_LINES;
    int start_line = TERM_LINES - max_vis;

    for (int i = 0; i < max_vis; i++) {
        int sy = wy + 46 + i * 18;
        if (lines[start_line + i][0] != '\0') {
            screen_str(wx + 16, sy, lines[start_line + i], 0xE0E6ED);
        }
    }

    int py = wy + wh - 28;
    screen_str(wx + 16, py, cur_dir, vfs_is_root() ? 0xFF5555 : 0x55CCFF);
    screen_str(wx + 144, py, ">", 0x00FF00);
    screen_str(wx + 168, py, cmd, 0xFFFFFF);
    screen_str(wx + 168 + cmd_len * 16, py, "_", 0x00FF00);
}