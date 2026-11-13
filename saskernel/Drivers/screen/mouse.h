#ifndef MOUSE_H
#define MOUSE_H

#include "../disk/types.h"

typedef struct {
    int x;
    int y;
    uint8_t left_btn;
    uint8_t right_btn;
} mouse_state_t;

void mouse_init(void);
void mouse_poll(void);
mouse_state_t mouse_get_state(void);
void mouse_draw_cursor(void);

#endif