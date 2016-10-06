#ifndef _SMAP_DMA_H_
#define _SMAP_DMA_H_


#include <types.h>
#include "smap.h"


struct smap_dma_transfer;
typedef void (*smap_dma_cb)(struct smap_dma_transfer *smaptr, void *arg);


struct smap_dma_transfer {
	void *addr;
	u32 size;
	u32 dir;
	u16 u16PTR;
	u8  u8Index;

	smap_dma_cb cb_done;
	void *cb_arg;
};


int  smap_dma_init(SMap* pSM);
int  smap_dma_transfer(struct smap_dma_transfer *smaptr);


#endif
