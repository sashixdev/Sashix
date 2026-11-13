#include "notify.h"
#include "screen.h"

#define TITLE_MAX 32
#define MSG_MAX   48

static char n_title[TITLE_MAX];
static char n_msg[MSG_MAX];
static int n_timer = 0;

static void n_cpy(char *dst, const char *src, int max) {
    int i = 0;
    while (*(src + i) && i < max - 1) {
        *(dst + i) = *(src + i);
        i++;
    }
    *(dst + i) = '\0';
}

void notify_push(const char *title, const char *msg) {
    n_cpy(n_title, title, TITLE_MAX);
    n_cpy(n_msg, msg, MSG_MAX);
    n_timer = 180;
}

void notify_draw(void) {
    if (n_timer <= 0) return;
    n_timer--;

    int nw = 460;
    int nh = 84;
    int nx = g_scr_w - nw - 18;
    int ny = g_scr_h - nh - 54;

    screen_rect_alpha(nx, ny, nw, nh, 0x1A2230, 230);
    screen_round_rect(nx, ny, nw, 6, 3, 0x00E5FF);
    screen_str(nx + 16, ny + 16, n_title, 0x58A6FF);
    screen_str(nx + 16, ny + 46, n_msg, 0xFFFFFF);
}