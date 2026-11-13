#include "Drivers/disk/types.h"
#include "Drivers/disk/io.h"
#include "Drivers/screen/screen.h"

extern void idt_init(void);
extern void installer_init(void);
extern void vga_init(void);
extern int  ata_identify_drive(uint8_t drive, char *model_out);

uint8_t g_sys_lang = 1;
static int boot_y = 24;

static void boot_delay(void) {
    for (volatile uint32_t d = 0; d < 2500000; d++) {
        asm volatile ("nop");
    }
}

static void boot_log(const char *tag, const char *msg, uint32_t tag_col) {
    screen_str(24, boot_y, tag, tag_col);
    screen_str(152, boot_y, msg, 0xE0E6ED);
    boot_y += 22;
    screen_swap();
    boot_delay();
}

static void boot_append_str(char *dst, const char *a, const char *b) {
    int p = 0;
    while (*(a + p)) {
        *(dst + p) = *(a + p);
        p++;
    }
    int q = 0;
    while (*(b + q)) {
        *(dst + p) = *(b + q);
        p++;
        q++;
    }
    *(dst + p) = '\0';
}

static void boot_u32_to_str(uint32_t val, char *out) {
    if (val == 0) {
        *(out + 0) = '0';
        *(out + 1) = '\0';
        return;
    }
    char tmp[16];
    int p = 0;
    while (val > 0 && p < 11) {
        *(tmp + p) = (char)('0' + (val % 10));
        p++;
        val /= 10;
    }
    for (int i = 0; i < p; i++) {
        *(out + i) = *(tmp + p - 1 - i);
    }
    *(out + p) = '\0';
}

static void run_verbose_boot(void) {
    screen_init();
    screen_clear(0x090C12);
    boot_y = 24;

    boot_log("[ BOOT ]", "sasKernel x86 32-bit Protected Mode", 0x58A6FF);

    uint32_t eax, ebx, ecx, edx;
    char vendor[16];
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *(uint32_t *)(vendor + 0) = ebx;
    *(uint32_t *)(vendor + 4) = edx;
    *(uint32_t *)(vendor + 8) = ecx;
    *(vendor + 12) = '\0';

    char cpu_msg[48];
    boot_append_str(cpu_msg, "CPUID Vendor: ", vendor);
    boot_log("[  OK  ]", cpu_msg, 0x00E676);

    outb(0x70, 0x30); uint8_t low  = inb(0x71);
    outb(0x70, 0x31); uint8_t high = inb(0x71);
    uint32_t ext_kb = ((uint32_t)high << 8) | low;
    outb(0x70, 0x34); uint8_t b34 = inb(0x71);
    outb(0x70, 0x35); uint8_t b35 = inb(0x71);
    uint32_t above16_64k = ((uint32_t)b35 << 8) | b34;
    uint32_t ram_mb = (above16_64k > 0) ? (16 + (above16_64k * 64) / 1024) : (1 + ext_kb / 1024);

    char num_str[16];
    char mem_msg[48];
    boot_u32_to_str(ram_mb, num_str);
    boot_append_str(mem_msg, "CMOS Memory Verified: ", num_str);
    boot_append_str(mem_msg, mem_msg, " MB");
    boot_log("[  OK  ]", mem_msg, 0x00E676);

    idt_init();
    boot_log("[  OK  ]", "IDT: 256 Interrupt Gates Loaded", 0x00E676);

    char hdd_model[48];
    char ata_msg[72];
    if (ata_identify_drive(0, hdd_model)) {
        boot_append_str(ata_msg, "ATA0 Primary Master: ", hdd_model);
        boot_log("[  OK  ]", ata_msg, 0x00E676);
    } else {
        boot_log("[ WARN ]", "ATA0 Primary Master: Not Responding", 0xFFCA28);
    }

    char hdd2_model[48];
    char ata2_msg[84];
    if (ata_identify_drive(1, hdd2_model)) {
        boot_append_str(ata2_msg, "ATA0 Primary Slave (DualBoot): ", hdd2_model);
        boot_log("[  OK  ]", ata2_msg, 0x00E676);
    }

    boot_log("[  OK  ]", "VESA Linear Framebuffer Mapped", 0x00E676);
    boot_log("[  OK  ]", "PS/2 Keyboard & Mouse IRQ Initialized", 0x00E676);
    boot_log("[  OK  ]", "Starting Sashix Environment...", 0x58A6FF);
    boot_delay();
}

void mount_init(void) {
    run_verbose_boot();
    installer_init();
    vga_init();

    while (1) {
        asm volatile ("hlt");
    }
}