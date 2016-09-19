#ifndef PATA_PS2_DEV9_H
#define PATA_PS2_DEV9_H


#include "types.h"


//#define USE_PS2SDK_DEV9
typedef void (*block_done_callback)(void *addr, u32 size, void *arg);


void pata_ps2_dev9_set_dir(int dir);
#ifndef USE_PS2SDK_DEV9
void pata_ps2_dev9_transfer(void *addr, u32 size, u32 write, block_done_callback cb, void *cb_arg);
#endif
int  pata_ps2_dev9_init();


#endif
