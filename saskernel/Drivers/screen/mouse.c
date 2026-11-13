#include "mouse.h"
#include "screen.h"
#include "../disk/io.h"

#define SPRITE_DIM 12

static mouse_state_t state = { 512, 384, 0, 0 };
static uint8_t cycle = 0;
static int8_t packet[3];

static const uint8_t cursor_sprite[SPRITE_DIM][SPRITE_DIM] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,1,1,1,1,0,0,0},
    {1,2,2,1,2,1,0,0,0,0,0,0},
    {1,1,0,0,1,2,1,0,0,0,0,0},
    {0,0,0,0,0,1,1,0,0,0,0,0}
};

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static void mouse_flush(void) {
    uint32_t timeout = 1000;
    while (timeout-- && (inb(0x64) & 1)) {
        inb(0x60);
    }
}

static void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init(void) {
    uint8_t status;
    cycle = 0;
    state.x = g_scr_w / 2;
    state.y = g_scr_h / 2;
    state.left_btn = 0;
    state.right_btn = 0;

    mouse_flush();

    // Включаем второй порт PS/2 (мышь)
    mouse_wait(1);
    outb(0x64, 0xA8);

    // Читаем байт конфигурации контроллера
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = inb(0x60);

    // Включаем IRQ12 (бит 1), трансляцию Set 1 (бит 6) и снимаем запрет тактирования мыши (бит 5)
    status = (status | 0x42) & ~0x20;
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    // Сброс настроек мыши по умолчанию и разрешение передачи пакетов
    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    mouse_flush();
}

void mouse_poll(void) {
    while (inb(0x64) & 1) {
        uint8_t status = inb(0x64);
        if (!(status & 0x20)) break;

        uint8_t data = inb(0x60);
        if (cycle == 0 && !(data & 0x08)) continue;

        packet[cycle] = (int8_t)data;
        cycle++;

        if (cycle == 3) {
            cycle = 0;
            uint8_t flags = (uint8_t)packet[0];
            if (flags & 0xC0) continue;

            state.left_btn = (flags & 0x01) ? 1 : 0;
            state.right_btn = (flags & 0x02) ? 1 : 0;

            int dx = packet[1];
            int dy = packet[2];

            state.x += dx;
            state.y -= dy;

            if (state.x < 0) state.x = 0;
            if (state.x > g_scr_w - SPRITE_DIM * 2) state.x = g_scr_w - SPRITE_DIM * 2;
            if (state.y < 0) state.y = 0;
            if (state.y > g_scr_h - SPRITE_DIM * 2) state.y = g_scr_h - SPRITE_DIM * 2;
        }
    }
}

mouse_state_t mouse_get_state(void) {
    return state;
}

void mouse_draw_cursor(void) {
    for (int y = 0; y < SPRITE_DIM; y++) {
        for (int x = 0; x < SPRITE_DIM; x++) {
            uint8_t pix = cursor_sprite[y][x];
            if (pix == 1) screen_rect(state.x + x * 2, state.y + y * 2, 2, 2, 0x000000);
            else if (pix == 2) screen_rect(state.x + x * 2, state.y + y * 2, 2, 2, 0xFFFFFF);
        }
    }
}