#include "vfs.h"
#include "Drivers/disk/io.h"
#include "Drivers/screen/notify.h"

extern void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer);
extern void ata_write_sector_drv(uint8_t drive, uint32_t lba, const uint16_t *source_buffer);

#define VFS_START_LBA 152
#define MAX_NODES     24
#define SEC_WORDS     256

static vfs_node_t nodes[MAX_NODES];
static int node_count = 0;
static int root_mode = 0;
static uint32_t saved_checksum = 0;

static void v_cpy(char *dst, const char *src, int max) {
    int i = 0;
    while (*(src + i) && i < max - 1) {
        *(dst + i) = *(src + i);
        i++;
    }
    *(dst + i) = '\0';
}

static int v_eq(const char *a, const char *b) {
    int i = 0;
    while (*(a + i) && *(b + i)) {
        if (*(a + i) != *(b + i)) return 0;
        i++;
    }
    return *(a + i) == *(b + i);
}

static uint32_t calc_vfs_checksum(void) {
    uint32_t sum = 0x12345678;
    for (int i = 0; i < node_count; i++) {
        sum ^= (nodes[i].size + (uint32_t)i * 31);
        for (int j = 0; nodes[i].name[j]; j++) {
            sum = (sum << 3) ^ (uint8_t)nodes[i].name[j];
        }
    }
    return sum;
}

static void save_node_to_disk(int idx) {
    if (idx < 0 || idx >= MAX_NODES) return;
    uint16_t sec[SEC_WORDS];
    for (int i = 0; i < SEC_WORDS; i++) *(sec + i) = 0;

    uint8_t *raw = (uint8_t *)sec;
    *(raw + 0) = 'V';
    *(raw + 1) = 'F';
    *(raw + 2) = 'S';
    *(raw + 3) = '2';

    uint8_t *src = (uint8_t *)&nodes[idx];
    uint32_t sz = sizeof(vfs_node_t);
    if (sz > 500) sz = 500;
    for (uint32_t i = 0; i < sz; i++) {
        *(raw + 8 + i) = *(src + i);
    }

    ata_write_sector_drv(0, VFS_START_LBA + (uint32_t)idx, sec);
}

static void create_default_files(void) {
    node_count = 0;
    vfs_write_file("/Sashix", "kernel.cfg", VFS_TYPE_FILE, 1, "SashixFS v2 Mounted", 19);
    vfs_write_file("/Users", "readme.txt", VFS_TYPE_FILE, 0, "Welcome to Sashix OS!", 21);

    const char *hello_sas =
        "// Hello App\n"
        "notify(\"Sashix\", \"App OK!\");\n"
        "beep(660);\n";
    int len = 0;
    while (*(hello_sas + len)) len++;
    vfs_write_file("/Apps", "hello.sas", VFS_TYPE_APP, 0, hello_sas, (uint32_t)len);
}

void vfs_init(void) {
    node_count = 0;
    root_mode = 0;

    uint16_t sec[SEC_WORDS];
    for (int i = 0; i < MAX_NODES; i++) {
        ata_read_sector_drv(0, VFS_START_LBA + (uint32_t)i, sec);
        uint8_t *raw = (uint8_t *)sec;
        if (*(raw + 0) == 'V' && *(raw + 1) == 'F' && *(raw + 2) == 'S' && *(raw + 3) == '2') {
            uint8_t *dst = (uint8_t *)&nodes[node_count];
            uint32_t sz = sizeof(vfs_node_t);
            if (sz > 500) sz = 500;
            for (uint32_t b = 0; b < sz; b++) {
                *(dst + b) = *(raw + 8 + b);
            }
            node_count++;
        }
    }

    if (node_count == 0) {
        create_default_files();
    }

    saved_checksum = calc_vfs_checksum();
}

void vfs_check_integrity(void) {
    uint32_t cur = calc_vfs_checksum();
    if (cur != saved_checksum) {
        saved_checksum = cur;
    }
}

int vfs_count(void) {
    return node_count;
}

vfs_node_t *vfs_get_node(int idx) {
    if (idx < 0 || idx >= node_count) return 0;
    return &nodes[idx];
}

vfs_node_t *vfs_find(const char *dir, const char *name) {
    for (int i = 0; i < node_count; i++) {
        if (v_eq(nodes[i].dir, dir) && v_eq(nodes[i].name, name)) {
            return &nodes[i];
        }
    }
    return 0;
}

int vfs_write_file(const char *dir, const char *name, uint8_t type, uint8_t is_sys, const void *data, uint32_t size) {
    vfs_node_t *ex = vfs_find(dir, name);
    int idx = -1;

    if (ex) {
        if (ex->is_system && !root_mode) return -1;
        idx = (int)(ex - nodes);
    } else {
        if (node_count >= MAX_NODES) return -2;
        idx = node_count++;
    }

    v_cpy(nodes[idx].dir, dir, sizeof(nodes[idx].dir));
    v_cpy(nodes[idx].name, name, sizeof(nodes[idx].name));
    nodes[idx].type = type;
    nodes[idx].is_system = is_sys;

    uint32_t max_sz = sizeof(nodes[idx].data) - 1;
    if (size > max_sz) size = max_sz;
    nodes[idx].size = size;

    const uint8_t *src = (const uint8_t *)data;
    for (uint32_t i = 0; i < size; i++) {
        nodes[idx].data[i] = *(src + i);
    }
    nodes[idx].data[size] = '\0';

    save_node_to_disk(idx);
    saved_checksum = calc_vfs_checksum();
    return 0;
}

int vfs_delete_file(const char *dir, const char *name) {
    vfs_node_t *ex = vfs_find(dir, name);
    if (!ex) return -2;
    if (ex->is_system && !root_mode) return -1;

    int idx = (int)(ex - nodes);
    for (int i = idx; i < node_count - 1; i++) {
        nodes[i] = nodes[i + 1];
        save_node_to_disk(i);
    }
    node_count--;

    uint16_t empty[SEC_WORDS];
    for (int i = 0; i < SEC_WORDS; i++) *(empty + i) = 0;
    ata_write_sector_drv(0, VFS_START_LBA + (uint32_t)node_count, empty);

    saved_checksum = calc_vfs_checksum();
    return 0;
}

int vfs_is_root(void) {
    return root_mode;
}

void vfs_set_root(int val) {
    root_mode = val ? 1 : 0;
}