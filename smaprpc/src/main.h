#ifndef _MAIN_H_
#define _MAIN_H_


#include "smap.h"
#include "smap_rpc.h"
#include "pbuf.h"
#include "types.h"
#include "stdio.h"


#define MODNAME "smaprpc"

#define M_PRINTF(format, args...) printf(MODNAME ": " format, ## args)
#define M_ERROR(format, args...) M_PRINTF("ERROR: " format, ## args)
#if 1
	#define M_DEBUG M_PRINTF
#else
	#define M_DEBUG(format, args...)
#endif


#define DMA_TRANSFER_SIZE 64

void smap_reset(void);
void SMapLowLevelInput(PBuf* pBuf);
SMapStatus SMapLowLevelOutput(PBuf* pBuf);

extern u32 ee_buffer;
extern u32 ee_buffer_pos;
extern volatile u32 ee_buffer_size;


#endif
