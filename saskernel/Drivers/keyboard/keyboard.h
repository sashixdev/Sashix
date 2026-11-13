#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../disk/types.h"

void keyboard_init(void);
uint8_t keyboard_read_scancode(void);
uint8_t keyboard_get_shift(void);
uint8_t keyboard_get_alt(void);
uint8_t keyboard_get_lang(void);
void keyboard_toggle_lang(void);
const char *keyboard_scancode_to_utf8(uint8_t sc, uint8_t shift);

#endif