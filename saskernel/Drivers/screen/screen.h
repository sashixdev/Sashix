#ifndef SASHIX_SCREEN_H
#define SASHIX_SCREEN_H

#include "../disk/types.h"

extern int g_scr_w;
extern int g_scr_h;

void screen_init(void);
void screen_clear(uint32_t color);
void screen_rect(int x, int y, int w, int h, uint32_t color);
void screen_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void screen_round_rect(int x, int y, int w, int h, int r, uint32_t color);
void screen_char(int x, int y, char c, uint32_t color);
void screen_str(int x, int y, const char *s, uint32_t color);
void screen_set_brightness(uint8_t pct);
void screen_swap(void);

#endif