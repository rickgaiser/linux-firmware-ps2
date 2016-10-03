#ifndef PATA_PS2_ATA_H
#define PATA_PS2_ATA_H


#include "types.h"
#include "pata_ps2_dev9.h"


#define ATA_MAX_TRANSFER_SIZE	(16*1024)


void pata_ps2_ata_set_dir(int dir);
void pata_ps2_ata_transfer(void *addr, u32 size, u32 write, block_done_callback cb, void *cb_arg);
int  pata_ps2_ata_init();


#endif
