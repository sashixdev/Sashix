#include "sas_vm.h"
#include "Drivers/disk/io.h"
#include "Drivers/screen/screen.h"
#include "Drivers/screen/notify.h"

extern uint8_t g_sys_lang;

#define OP_RECT   0x01
#define OP_TEXT   0x02
#define OP_BEEP   0x03
#define OP_NOTIFY 0x04
#define OP_WAIT   0x05
#define OP_END    0xFF

#define BC_MAX    1024
#define LINE_BUF  128
#define TXT_BUF   64
#define T1_BUF    32
#define T2_BUF    48

static int s_starts(const char *s, const char *p) {
    int i = 0;
    while (*(p + i)) {
        if (*(s + i) != *(p + i)) return 0;
        i++;
    }
    return 1;
}

static uint32_t parse_num(const char **pp) {
    const char *p = *pp;
    while (*p == ' ' || *p == ',' || *p == '(' || *p == '\t') p++;
    uint32_t val = 0;
    if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) {
        p += 2;
        while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
            val <<= 4;
            if (*p >= '0' && *p <= '9') val |= (uint32_t)(*p - '0');
            else if (*p >= 'a' && *p <= 'f') val |= (uint32_t)(*p - 'a' + 10);
            else val |= (uint32_t)(*p - 'A' + 10);
            p++;
        }
    } else {
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (uint32_t)(*p - '0');
            p++;
        }
    }
    *pp = p;
    return val;
}

static int parse_str(const char **pp, char *out, int max) {
    const char *p = *pp;
    while (*p && *p != '"') p++;
    if (*p != '"') return 0;
    p++;
    int len = 0;
    while (*p && *p != '"' && len < max - 1) {
        *(out + len++) = *p++;
    }
    *(out + len) = '\0';
    if (*p == '"') p++;
    *pp = p;
    return len;
}

static void emit_u32(uint8_t *buf, uint32_t *pos, uint32_t v) {
    if (*pos + 4 >= BC_MAX) return;
    *(uint32_t *)(buf + *pos) = v;
    *pos += 4;
}

int sas_compile_text(const char *src_text, uint32_t src_len, const char *out_name) {
    uint8_t bc[BC_MAX];
    uint32_t bp = 0;

    *(bc + bp++) = 'S';
    *(bc + bp++) = 'A';
    *(bc + bp++) = 'S';
    *(bc + bp++) = 'H';
    *(bc + bp++) = 1;
    *(bc + bp++) = 0;
    *(bc + bp++) = 0;
    *(bc + bp++) = 0;

    char line[LINE_BUF];
    uint32_t sp = 0;

    while (sp <= src_len) {
        int lp = 0;
        while (sp < src_len && *(src_text + sp) != '\n' && lp < LINE_BUF - 2) {
            *(line + lp++) = *(src_text + sp++);
        }
        *(line + lp) = '\0';
        sp++;

        const char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r') p++;
        if (*p == '\0' || (*p == '/' && *(p + 1) == '/')) continue;

        if (s_starts(p, "rect(")) {
            p += 5;
            uint32_t x = parse_num(&p);
            uint32_t y = parse_num(&p);
            uint32_t w = parse_num(&p);
            uint32_t h = parse_num(&p);
            uint32_t c = parse_num(&p);
            *(bc + bp++) = OP_RECT;
            emit_u32(bc, &bp, x);
            emit_u32(bc, &bp, y);
            emit_u32(bc, &bp, w);
            emit_u32(bc, &bp, h);
            emit_u32(bc, &bp, c);
        } else if (s_starts(p, "text(")) {
            p += 5;
            uint32_t x = parse_num(&p);
            uint32_t y = parse_num(&p);
            char txt[TXT_BUF];
            int tlen = parse_str(&p, txt, TXT_BUF - 4);
            uint32_t c = parse_num(&p);
            *(bc + bp++) = OP_TEXT;
            emit_u32(bc, &bp, x);
            emit_u32(bc, &bp, y);
            emit_u32(bc, &bp, c);
            *(bc + bp++) = (uint8_t)tlen;
            for (int i = 0; i < tlen && bp < BC_MAX - 2; i++) *(bc + bp++) = (uint8_t)*(txt + i);
        } else if (s_starts(p, "beep(")) {
            p += 5;
            uint32_t freq = parse_num(&p);
            *(bc + bp++) = OP_BEEP;
            emit_u32(bc, &bp, freq);
        } else if (s_starts(p, "notify(")) {
            p += 7;
            char t1[T1_BUF], t2[T2_BUF];
            int l1 = parse_str(&p, t1, T1_BUF);
            int l2 = parse_str(&p, t2, T2_BUF);
            *(bc + bp++) = OP_NOTIFY;
            *(bc + bp++) = (uint8_t)l1;
            for (int i = 0; i < l1 && bp < BC_MAX - 2; i++) *(bc + bp++) = (uint8_t)*(t1 + i);
            *(bc + bp++) = (uint8_t)l2;
            for (int i = 0; i < l2 && bp < BC_MAX - 2; i++) *(bc + bp++) = (uint8_t)*(t2 + i);
        } else if (s_starts(p, "wait(")) {
            *(bc + bp++) = OP_WAIT;
        }
    }

    *(bc + bp++) = OP_END;
    return vfs_write_file("/Apps", out_name, VFS_TYPE_APP, 0, bc, bp);
}

static void vm_beep(uint32_t freq) {
    if (freq < 50 || freq > 5000) return;
    uint32_t div = 1193180 / freq;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) outb(0x61, tmp | 3);
    for (volatile uint32_t d = 0; d < 350000; d++) asm volatile ("nop");
    outb(0x61, inb(0x61) & 0xFC);
}

void sas_exec_node(vfs_node_t *node) {
    if (!node || node->size < 9 ||
        *(node->data + 0) != 'S' || *(node->data + 1) != 'A' ||
        *(node->data + 2) != 'S' || *(node->data + 3) != 'H') {
        notify_push("Sashix VM", "Invalid .sas binary!");
        return;
    }

    uint32_t pc = 8;
    const uint8_t *code = node->data;

    while (pc < node->size) {
        uint8_t op = *(code + pc++);
        if (op == OP_END) break;

        if (op == OP_RECT && pc + 20 <= node->size) {
            int x = (int)*(uint32_t *)(code + pc); pc += 4;
            int y = (int)*(uint32_t *)(code + pc); pc += 4;
            int w = (int)*(uint32_t *)(code + pc); pc += 4;
            int h = (int)*(uint32_t *)(code + pc); pc += 4;
            uint32_t c = *(uint32_t *)(code + pc); pc += 4;
            screen_rect(x, y, w, h, c);
        } else if (op == OP_TEXT && pc + 13 <= node->size) {
            int x = (int)*(uint32_t *)(code + pc); pc += 4;
            int y = (int)*(uint32_t *)(code + pc); pc += 4;
            uint32_t c = *(uint32_t *)(code + pc); pc += 4;
            uint8_t len = *(code + pc++);
            char buf[TXT_BUF];
            int i = 0;
            while (i < len && i < TXT_BUF - 1 && pc < node->size) *(buf + i++) = (char)*(code + pc++);
            *(buf + i) = '\0';
            screen_str(x, y, buf, c);
        } else if (op == OP_BEEP && pc + 4 <= node->size) {
            uint32_t f = *(uint32_t *)(code + pc); pc += 4;
            vm_beep(f);
        } else if (op == OP_NOTIFY && pc + 2 <= node->size) {
            uint8_t l1 = *(code + pc++);
            char t1[T1_BUF];
            int i = 0;
            while (i < l1 && i < T1_BUF - 1 && pc < node->size) *(t1 + i++) = (char)*(code + pc++);
            *(t1 + i) = '\0';
            uint8_t l2 = *(code + pc++);
            char t2[T2_BUF];
            i = 0;
            while (i < l2 && i < T2_BUF - 1 && pc < node->size) *(t2 + i++) = (char)*(code + pc++);
            *(t2 + i) = '\0';
            notify_push(t1, t2);
        } else if (op == OP_WAIT) {
            screen_str(340, 680, g_sys_lang ? "[ Нажмите ESC для выхода ]" : "[ Press ESC to exit ]", 0xFFFF00);
            screen_swap();
            while (1) {
                if (inb(0x64) & 1) {
                    uint8_t sc = inb(0x60);
                    if (sc == 0x01 || sc == 0x1C) break;
                }
            }
        } else {
            break;
        }
    }
    screen_swap();
}