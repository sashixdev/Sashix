#include "keyboard.h"
#include "../disk/io.h"
#include "../screen/notify.h"

#define UTF8_BUF_LEN 4
#define RU_KEYS_CNT  52

uint8_t g_kbd_lang = 0; // 0 = ENG, 1 = РУС (только раскладка ввода)
static uint8_t shift_pressed = 0;
static uint8_t alt_pressed = 0;

static void kbd_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout-- && (inb(0x64) & 2));
}

static void kbd_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout-- && !(inb(0x64) & 1));
}

void keyboard_init(void) {
    shift_pressed = 0;
    alt_pressed = 0;

    kbd_wait_write();
    outb(0x64, 0x20);
    kbd_wait_read();
    uint8_t cfg = inb(0x60);

    cfg |= 0x41; // Включаем аппаратную трансляцию в Scan Code Set 1 и IRQ1

    kbd_wait_write();
    outb(0x64, 0x60);
    kbd_wait_write();
    outb(0x60, cfg);

    while (inb(0x64) & 1) {
        inb(0x60);
    }
}

uint8_t keyboard_get_shift(void) { return shift_pressed; }
uint8_t keyboard_get_alt(void)   { return alt_pressed; }
uint8_t keyboard_get_lang(void)  { return g_kbd_lang; }

void keyboard_toggle_lang(void) {
    g_kbd_lang = !g_kbd_lang;
    notify_push("Keyboard (F6)", g_kbd_lang ? "Layout: RUS" : "Layout: ENG");
}

uint8_t keyboard_read_scancode(void) {
    uint8_t status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) return 0;

    uint8_t sc = inb(0x60);
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; return 0; }
    if (sc == 0x38) { alt_pressed = 1; return sc; }
    if (sc == 0xB8) { alt_pressed = 0; return 0; }

    if (sc & 0x80) return 0;

    if (sc == 0x40) {
        keyboard_toggle_lang();
        return 0;
    }
    return sc;
}

const char *keyboard_scancode_to_utf8(uint8_t sc, uint8_t shift) {
    static char out[UTF8_BUF_LEN];
    *(out + 0) = '\0';

    if (sc == 0x39) {
        *(out + 0) = ' ';
        *(out + 1) = '\0';
        return out;
    }
    if (sc < 0x02 || sc > 0x35) return out;

    const char *en_low  = "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./";
    const char *en_high = "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?";

    if (!g_kbd_lang) {
        char c = shift ? *(en_high + (sc - 0x02)) : *(en_low + (sc - 0x02));
        if (c >= 33 && c <= 126) {
            *(out + 0) = c;
            *(out + 1) = '\0';
        }
        return out;
    }

    const char *ru_low[RU_KEYS_CNT] = {
        "1","2","3","4","5","6","7","8","9","0","-","=","","",
        "й","ц","у","к","е","н","г","ш","щ","з","х","ъ","","",
        "ф","ы","в","а","п","р","о","л","д","ж","э","ё","","\\",
        "я","ч","с","м","и","т","ь","б","ю","."
    };
    const char *ru_high[RU_KEYS_CNT] = {
        "!","\"","N",";","%",":","?","*","(",")","_","+","","",
        "Й","Ц","У","К","Е","Н","Г","Ш","Щ","З","Х","Ъ","","",
        "Ф","Ы","В","А","П","Р","О","Л","Д","Ж","Э","Ё","","/",
        "Я","Ч","С","М","И","Т","Ь","Б","Ю",","
    };

    const char *glyph = shift ? *(ru_high + (sc - 0x02)) : *(ru_low + (sc - 0x02));
    int i = 0;
    while (*(glyph + i) && i < UTF8_BUF_LEN - 1) {
        *(out + i) = *(glyph + i);
        i++;
    }
    *(out + i) = '\0';
    return out;
}