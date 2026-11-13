#ifndef SASHIX_SAS_VM_H
#define SASHIX_SAS_VM_H

#include "Drivers/disk/types.h"
#include "vfs.h"

int  sas_compile_text(const char *src_text, uint32_t src_len, const char *out_name);
void sas_exec_node(vfs_node_t *node);

#endif