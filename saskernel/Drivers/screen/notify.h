#ifndef SASHIX_NOTIFY_H
#define SASHIX_NOTIFY_H

#include "Drivers/disk/types.h"

void notify_push(const char *title, const char *msg);
void notify_draw(void);

#endif