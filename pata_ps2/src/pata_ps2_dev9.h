#ifndef PATA_PS2_DEV9_H
#define PATA_PS2_DEV9_H


#include "types.h"


typedef void (*block_done_callback)(void *addr, u32 size, void *arg);
typedef void (*dev9_dma_done_callback)(void *addr);


void pata_ps2_dev9_set_dir(int dir);
int  pata_ps2_dev9_transfer(int ctrl, void *addr, int bcr, int dir, dev9_dma_done_callback cb, void *cb_arg);
int  pata_ps2_dev9_init();


#endif
