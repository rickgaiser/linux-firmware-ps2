#ifndef PATA_PS2_CORE_H
#define PATA_PS2_CORE_H


#include "types.h"


typedef void (*block_done_callback)(void *addr, u32 size, void *arg);


void pata_ps2_core_set_dir(int dir);
void pata_ps2_core_transfer(void *addr, u32 size, u32 write, block_done_callback cb, void *cb_arg);
int  pata_ps2_core_init();


#endif
