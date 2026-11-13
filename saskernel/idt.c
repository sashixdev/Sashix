#include "Drivers/disk/types.h"
#include "Drivers/disk/io.h"

#define IDT_ENTRIES 256

typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idtp;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void kernel_panic(void *regs);

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

void isr_handler(void *regs) {
    kernel_panic(regs);
}

void idt_init(void) {
    uint16_t cs = 0x08;
    asm volatile ("mov %%cs, %0" : "=r"(cs));

    idtp.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    idt_set_gate(0,  (uint32_t)isr0,  cs, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  cs, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  cs, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  cs, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  cs, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  cs, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  cs, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  cs, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  cs, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  cs, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, cs, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, cs, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, cs, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, cs, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, cs, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, cs, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, cs, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, cs, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, cs, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, cs, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, cs, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, cs, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, cs, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, cs, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, cs, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, cs, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, cs, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, cs, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, cs, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, cs, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, cs, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, cs, 0x8E);

    asm volatile ("lidt %0" : : "m"(idtp));
}