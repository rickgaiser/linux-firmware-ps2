#ifndef PATA_PS2_BUFFER_H
#define PATA_PS2_BUFFER_H


#include "types.h"
#include "pata_ps2_dev9.h"


/*
 * Make the ring-buffer large enough so the SPEED->IOP transfer will not
 * overwrite the IOP->EE transfer.
 *
 * The transfer to EE is about 4 times faster, so locking is not needed.
 */
#define BUFFER_MAX_TRANSFER_SIZE	(16*1024)
#define DATA_BUFFER_SIZE		(4*BUFFER_MAX_TRANSFER_SIZE)
extern u8 _data_buffer[DATA_BUFFER_SIZE];


void pata_ps2_buffer_read(u32 size, block_done_callback cb, void *cb_arg);
int  pata_ps2_buffer_init();


#endif
