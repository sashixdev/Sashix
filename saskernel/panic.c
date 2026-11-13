#include "Drivers/disk/types.h"
#include "Drivers/screen/screen.h"

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} regs_t;

static void p_hex(uint32_t val, char *out) {
    const char *d = "0123456789ABCDEF";
    *(out + 0) = '0'; *(out + 1) = 'x';
    for (int i = 0; i < 8; i++) *(out + 9 - i) = *(d + ((val >> (i * 4)) & 0x0F));
    *(out + 10) = '\0';
}

static const char *exc_name(uint32_t n) {
    if (n == 0)  return "0: Divide By Zero";
    if (n == 3)  return "3: Critical Breakpoint / Panic";
    if (n == 6)  return "6: Invalid Opcode";
    if (n == 8)  return "8: Double Fault";
    if (n == 13) return "13: General Protection Fault";
    if (n == 14) return "14: Page Fault";
    return "Fatal Kernel Exception";
}

void kernel_panic(regs_t *r) {
    asm volatile ("cli");
    int cx = g_scr_w / 2;
    int cy = g_scr_h / 2;

    screen_clear(0x14080C);
    screen_round_rect(cx - 320, cy - 200, 640, 400, 14, 0x220E14);
    screen_round_rect(cx - 320, cy - 200, 640, 46, 14, 0xB71C1C);

    screen_str(cx - 296, cy - 185, ">_ SASHIX KERNEL PANIC", 0xFFFFFF);
    screen_str(cx - 296, cy - 130, exc_name(r->int_no), 0xFF8A80);

    char hb[12];
    p_hex(r->eip, hb); screen_str(cx - 296, cy - 76, "EIP:", 0xAAAAAA); screen_str(cx - 200, cy - 76, hb, 0x00FFAA);
    p_hex(r->eax, hb); screen_str(cx - 296, cy - 42, "EAX:", 0xAAAAAA); screen_str(cx - 200, cy - 42, hb, 0xFFFFFF);
    p_hex(r->ebx, hb); screen_str(cx - 296, cy - 8,  "EBX:", 0xAAAAAA); screen_str(cx - 200, cy - 8,  hb, 0xFFFFFF);
    p_hex(r->ecx, hb); screen_str(cx - 296, cy + 26, "ECX:", 0xAAAAAA); screen_str(cx - 200, cy + 26, hb, 0xFFFFFF);

    p_hex(r->edx, hb); screen_str(cx + 20, cy - 76, "EDX:", 0xAAAAAA); screen_str(cx + 116, cy - 76, hb, 0xFFFFFF);
    p_hex(r->esp, hb); screen_str(cx + 20, cy - 42, "ESP:", 0xAAAAAA); screen_str(cx + 116, cy - 42, hb, 0xFFFF00);
    p_hex(r->ebp, hb); screen_str(cx + 20, cy - 8,  "EBP:", 0xAAAAAA); screen_str(cx + 116, cy - 8,  hb, 0xFFFF00);
    p_hex(r->err_code, hb); screen_str(cx + 20, cy + 26, "ERR:", 0xAAAAAA); screen_str(cx + 116, cy + 26, hb, 0xFF5252);

    screen_str(cx - 296, cy + 140, "System halted. Reboot PC.", 0x8B949E);
    screen_swap();

    while (1) {
        asm volatile ("hlt");
    }
}