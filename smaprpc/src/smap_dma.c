#include "smap_dma.h"
#include "main.h"

#include <dmacman.h>
#include "dev9_dma.h"


static struct dev9_dma_client dev9_smap;
static SMap* pSMap;


/*
 * private functions
 */
static void
_smap_dma_dev9_pre_dma(struct dev9_dma_transfer *dev9tr, void *arg){
	struct smap_dma_transfer *smaptr = (struct smap_dma_transfer *)arg;

	//M_DEBUG("%s:  slices=%lu\n", __func__, tr->bcr >> 16);

	if((dev9tr->chcr & DMAC_CHCR_DR) != DMAC_TO_MEM){
		SMAP_REG16(pSMap, SMAP_TXFIFO_WR_PTR) = smaptr->u16PTR;
		SMAP_REG16(pSMap, SMAP_TXFIFO_SIZE) = dev9tr->bcr >> 16;
		SMAP_REG8(pSMap, SMAP_TXFIFO_CTRL) = TXFIFO_DMAEN;
	}
	else{
		SMAP_REG16(pSMap, SMAP_RXFIFO_RD_PTR) = smaptr->u16PTR;
		SMAP_REG16(pSMap, SMAP_RXFIFO_SIZE) = dev9tr->bcr >> 16;
		SMAP_REG8(pSMap, SMAP_RXFIFO_CTRL) = RXFIFO_DMAEN;
	}
}

static void
_smap_dma_dev9_post_dma(struct dev9_dma_transfer *dev9tr, void *arg){
	struct smap_dma_transfer *smaptr = (struct smap_dma_transfer *)arg;
	int iPKTLenDMA = (dev9tr->bcr >> 16) << dev9tr->cl->block_shift;
	int iPKTLenAligned = (smaptr->size+3) & ~3;
	int i;

	//M_DEBUG("%s: slices=%lu\n", __func__, tr->bcr >> 16);

	if((dev9tr->chcr & DMAC_CHCR_DR) != DMAC_TO_MEM){
		while(SMAP_REG8(pSMap, SMAP_TXFIFO_CTRL) & TXFIFO_DMAEN){};

		if (iPKTLenDMA < iPKTLenAligned) {
			for (i = iPKTLenDMA; i < iPKTLenAligned; i += 4)
				SMAP_REG32(pSMap, SMAP_TXFIFO_DATA) = ((u32 *)smaptr->addr)[i/4];
		}
	}
	else{
		while(SMAP_REG8(pSMap, SMAP_RXFIFO_CTRL) & RXFIFO_DMAEN){};

		if (iPKTLenDMA < iPKTLenAligned) {
			for (i = iPKTLenDMA; i < iPKTLenAligned; i += 4)
				((u32 *)smaptr->addr)[i/4] = SMAP_REG32(pSMap, SMAP_RXFIFO_DATA);
		}
	}
}

static void
_smap_dma_dev9_done(struct dev9_dma_transfer *dev9tr, void *arg)
{
	struct smap_dma_transfer *smaptr = (struct smap_dma_transfer *)arg;

	//M_DEBUG("%s\n", __func__);

	if (smaptr->cb_done != NULL)
		smaptr->cb_done(smaptr, smaptr->cb_arg);
}

/*
 * public functions
 */
int
smap_dma_init(SMap* pSM)
{
	pSMap = pSM;

	/* 128 byte block transfer. (1 << 7) == 128 */
	if (dev9_dma_client_init(&dev9_smap, 1, 7) != 0) {
		return -1;
	}
	dev9_smap.cb_pre_dma = _smap_dma_dev9_pre_dma;
	dev9_smap.cb_post_dma = _smap_dma_dev9_post_dma;
	dev9_smap.cb_done = _smap_dma_dev9_done;

	return 0;
}

int
smap_dma_transfer(struct smap_dma_transfer *smaptr)
{
	int rv;

	rv = dev9_dma_transfer(&dev9_smap, smaptr->addr, smaptr->size, smaptr->dir, smaptr);
	if (rv != 0) {
		M_ERROR("%s: dev9_dma_transfer returned %d\n", __func__, rv);
		return -1;
	}

	return 0;
}
