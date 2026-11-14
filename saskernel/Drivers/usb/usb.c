typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycodes[6];
} __attribute__((packed)) usb_kbd_packet_t;

typedef struct {
    uint8_t buttons;
    int8_t  x_offset;
    int8_t  y_offset;
    int8_t  wheel;
} __attribute__((packed)) usb_mouse_packet_t;

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint32_t find_uhci_controller(void) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t reg0 = pci_read(bus, slot, func, 0x00);
                if (reg0 == 0xFFFFFFFF) continue;

                uint32_t class_reg = pci_read(bus, slot, func, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass   = (class_reg >> 16) & 0xFF;
                uint8_t prog_if    = (class_reg >> 8) & 0xFF;

                if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x00) {
                    uint32_t bar4 = pci_read(bus, slot, func, 0x20);
                    if (bar4 & 1) {
                        return bar4 & 0xFFFFFFFC;
                    }
                }
            }
        }
    }
    return 0;
}

void init_uhci_hardware(uint32_t io_base) {
    outl(io_base + 0x00, 0x0004);
    for (volatile int c = 0; c < 100000; c++);
    outl(io_base + 0x00, 0x0000);
    outl(io_base + 0x02, 0xFFFF);
}

void saskernel_usb_mouse_handler(usb_mouse_packet_t *packet) {
    if (!packet) return;
    int left = packet->buttons & 0x01;
    int right = packet->buttons & 0x02;
    
    extern int mouse_x, mouse_y;
    mouse_x += packet->x_offset;
    mouse_y -= packet->y_offset;
    
    extern void mouse_update(int x, int y, int l, int r);
    mouse_update(mouse_x, mouse_y, left, right);
}

void saskernel_usb_kbd_handler(usb_kbd_packet_t *packet) {
    if (!packet) return;
    extern void gui_keyboard_handle(uint8_t scancode);
    
    for (int i = 0; i < 6; i++) {
        uint8_t code = packet->keycodes[i];
        if (code == 0) continue;
        
        uint8_t legacy_code = 0;
        if (code >= 0x04 && code <= 0x1D) {
            legacy_code = code + 0x1A; 
        }
        else if (code == 0x28) legacy_code = 0x1C;
        else if (code == 0x29) legacy_code = 0x01;
        else if (code == 0x2A) legacy_code = 0x0E;
        else if (code == 0x2C) legacy_code = 0x39;
        else if (code == 0x52) legacy_code = 0x48;
        else if (code == 0x51) legacy_code = 0x50;
        else if (code == 0x50) legacy_code = 0x4B;
        else if (code == 0x4F) legacy_code = 0x4D;
        
        if (legacy_code) {
            gui_keyboard_handle(legacy_code);
        }
    }
}