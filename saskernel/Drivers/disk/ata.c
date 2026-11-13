#include "types.h"
#include "io.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRIVE_HEAD   0x1F6
#define ATA_PRIMARY_COMM_STAT    0x1F7
#define ATA_PRIMARY_ALT_STAT     0x3F6

#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_WRITE_PIO        0x30
#define ATA_CMD_CACHE_FLUSH      0xE7
#define ATA_CMD_IDENTIFY         0xEC

static void ata_io_delay(void) {
    inb(ATA_PRIMARY_ALT_STAT);
    inb(ATA_PRIMARY_ALT_STAT);
    inb(ATA_PRIMARY_ALT_STAT);
    inb(ATA_PRIMARY_ALT_STAT);
}

static int ata_wait_bsy(void) {
    uint32_t timeout = 500000;
    while (timeout--) {
        if (!(inb(ATA_PRIMARY_COMM_STAT) & 0x80)) return 1;
    }
    return 0;
}

static int ata_wait_drq(void) {
    uint32_t timeout = 500000;
    while (timeout--) {
        uint8_t st = inb(ATA_PRIMARY_COMM_STAT);
        if (st & 0x01) return 0;
        if (st & 0x08) return 1;
    }
    return 0;
}

void ata_read_sector_drv(uint8_t drive, uint32_t lba, uint16_t *target_buffer) {
    if (!ata_wait_bsy()) return;

    uint8_t drv_bit = (drive & 1) ? 0xF0 : 0xE0;
    outb(ATA_PRIMARY_DRIVE_HEAD, drv_bit | ((lba >> 24) & 0x0F));
    ata_io_delay();

    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_READ_PIO);

    if (!ata_wait_bsy() || !ata_wait_drq()) return;

    for (int i = 0; i < 256; i++) {
        *(target_buffer + i) = inw(ATA_PRIMARY_DATA);
    }
    ata_io_delay();
}

void ata_write_sector_drv(uint8_t drive, uint32_t lba, const uint16_t *source_buffer) {
    if (!ata_wait_bsy()) return;

    uint8_t drv_bit = (drive & 1) ? 0xF0 : 0xE0;
    outb(ATA_PRIMARY_DRIVE_HEAD, drv_bit | ((lba >> 24) & 0x0F));
    ata_io_delay();

    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_WRITE_PIO);

    if (!ata_wait_bsy() || !ata_wait_drq()) return;

    for (int i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_DATA, *(source_buffer + i));
    }

    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();
    ata_io_delay();
}

void ata_read_sector(uint32_t lba, uint16_t *target_buffer) {
    ata_read_sector_drv(0, lba, target_buffer);
}

void ata_write_sector(uint32_t lba, const uint16_t *source_buffer) {
    ata_write_sector_drv(0, lba, source_buffer);
}

int ata_identify_drive(uint8_t drive, char *model_out) {
    outb(ATA_PRIMARY_DRIVE_HEAD, (drive & 1) ? 0xB0 : 0xA0);
    ata_io_delay();

    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_PRIMARY_COMM_STAT);
    if (status == 0 || status == 0xFF) return 0;

    if (!ata_wait_bsy() || !ata_wait_drq()) return 0;

    uint16_t id[256];
    for (int i = 0; i < 256; i++) {
        *(id + i) = inw(ATA_PRIMARY_DATA);
    }

    int p = 0;
    for (int w = 27; w <= 46; w++) {
        uint16_t val = *(id + w);
        char c1 = (char)(val >> 8);
        char c2 = (char)(val & 0xFF);
        if (c1 >= 32 && c1 <= 126) *(model_out + p++) = c1;
        if (c2 >= 32 && c2 <= 126) *(model_out + p++) = c2;
    }
    while (p > 0 && *(model_out + p - 1) == ' ') p--;
    *(model_out + p) = '\0';

    return (p > 0) ? 1 : 0;
}