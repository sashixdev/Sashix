#ifndef SASHIX_VFS_H
#define SASHIX_VFS_H

#include "Drivers/disk/types.h"

#define VFS_MAX_FILES 32
#define VFS_NAME_LEN  24
#define VFS_DIR_LEN   16
#define VFS_DATA_MAX  2048

#define VFS_TYPE_FILE 1
#define VFS_TYPE_APP  2

typedef struct {
    char     name[VFS_NAME_LEN];
    char     dir[VFS_DIR_LEN];
    uint8_t  type;
    uint8_t  is_system;
    uint32_t size;
    uint8_t  data[VFS_DATA_MAX];
} vfs_node_t;

void vfs_init(void);
void vfs_sync(void);
void vfs_check_integrity(void);

int  vfs_is_root(void);
void vfs_set_root(int state);

int  vfs_count(void);
vfs_node_t *vfs_get_node(int idx);
vfs_node_t *vfs_find(const char *dir, const char *name);

int  vfs_write_file(const char *dir, const char *name, uint8_t type, uint8_t is_sys, const void *buf, uint32_t len);
int  vfs_delete_file(const char *dir, const char *name);

#endif