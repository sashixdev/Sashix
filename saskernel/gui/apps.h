#ifndef SASHIX_APPS_H
#define SASHIX_APPS_H

#include "../Drivers/disk/types.h"
#include "../Drivers/screen/mouse.h"

typedef struct {
    int tz_offset;
    uint32_t bg_color;
    uint8_t brightness;
    uint8_t volume;
} sys_settings_t;

extern sys_settings_t g_settings;
extern char g_clipboard[1024];

void gui_settings_init(void);
void gui_settings_open(void);
void gui_settings_close(void);
void gui_settings_update(mouse_state_t m, uint8_t click);
void gui_settings_draw_bg(void);
void gui_settings_draw(void);

void gui_term_init(void);
void gui_term_open(void);
void gui_term_close(void);
void gui_term_copy_clip(void);
void gui_term_paste_clip(void);
void gui_term_update(mouse_state_t m, uint8_t click, uint8_t sc, uint8_t ctrl, uint8_t shift, uint8_t alt);
void gui_term_draw(void);

void gui_files_init(void);
void gui_files_open(void);
void gui_files_close(void);
void gui_files_update(mouse_state_t m, uint8_t click);
void gui_files_draw(void);

void gui_sappedit_init(void);
void gui_sappedit_open(void);
void gui_sappedit_close(void);
void gui_sappedit_copy_clip(void);
void gui_sappedit_paste_clip(void);
void gui_sappedit_update(mouse_state_t m, uint8_t click, uint8_t sc, uint8_t ctrl, uint8_t shift);
void gui_sappedit_draw(void);

void gui_snake_open(void);
void gui_snake_close(void);
void gui_snake_update(mouse_state_t m, uint8_t click, uint8_t sc);
void gui_snake_draw(void);

void gui_3d_open(void);
void gui_3d_close(void);
void gui_3d_update(mouse_state_t m, uint8_t click, uint8_t sc);
void gui_3d_draw(void);

void gui_media_open_jpg(const char *name, uint32_t lba);
void gui_media_open_mp4(const char *name, uint32_t lba, uint16_t frames);
void gui_media_open_node(const char *name, const uint8_t *data, uint32_t size);
void gui_media_close(void);
void gui_media_update(mouse_state_t m, uint8_t click);
void gui_media_draw(void);

#endif