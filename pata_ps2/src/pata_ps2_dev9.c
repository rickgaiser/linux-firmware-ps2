#include "pata_ps2.h"
#include "pata_ps2_dev9.h"

#include "intrman.h"
#include "dmacman.h"

#include "dev9regs.h"
#include "speedregs.h"


struct dev9_transfer {
	int ctrl;
 	int dmactrl;
	void * addr;
	int bcr;
	int dir;
	dev9_dma_done_callback cb;
	void *cb_arg;
};
static struct dev9_transfer transfer;


/*
 * private functions
 */
static inline void
_dma_start(struct dev9_transfer *tr)
{
	USE_SPD_REGS;
	volatile iop_dmac_chan_t *dev9_chan = (iop_dmac_chan_t *)DEV9_DMAC_BASE;

	M_DEBUG("%s\n", __func__);

	/* Start DMA transfer: IOP <--> SPEED */
	SPD_REG16(SPD_R_XFR_CTRL)|=0x80;
	SPD_REG16(SPD_R_DMA_CTRL) = (SPD_REG16(SPD_R_REV_1)<17)?(tr->dmactrl&0x03)|0x04:(tr->dmactrl&0x01)|0x06;

	dev9_chan->madr = (u32)tr->addr;
	dev9_chan->bcr  = tr->bcr;
	dev9_chan->chcr = DMAC_CHCR_30|DMAC_CHCR_TR|DMAC_CHCR_CO|(tr->dir & DMAC_CHCR_DR);
}

static inline void
_dma_stop()
{
	USE_SPD_REGS;

	M_DEBUG("%s\n", __func__);

	SPD_REG16(SPD_R_XFR_CTRL)&=~0x80;
}

static int
_dma_intr_handler(void *arg)
{
	struct dev9_transfer *tr = arg;

	M_DEBUG("%s\n", __func__);

	_dma_stop();

	/* Call DMA done callback */
	if (tr->cb != NULL)
		tr->cb(tr->cb_arg);

	return 1;
}

/*
 * public functions
 */
void
pata_ps2_dev9_set_dir(int dir)
{
	USE_SPD_REGS;
	u16 val;

	M_DEBUG("%s\n", __func__);

	/* 0x38 ??: What does this do? this register also holds the number of blocks ready for DMA */
	SPD_REG16(0x38) = 3;

	/* IF_CTRL: Save first bit (0=MWDMA, 1=UDMA) */
	val = SPD_REG16(SPD_R_IF_CTRL) & 1;
	/* IF_CTRL: Set direction */
	val |= (dir == ATA_DIR_WRITE) ? 0x4c : 0x4e;
	SPD_REG16(SPD_R_IF_CTRL) = val;

	/* XFR_CTRL: Set direction */
	SPD_REG16(SPD_R_XFR_CTRL) = dir | 0x6;
}

int
pata_ps2_dev9_transfer(int ctrl, void *addr, int bcr, int dir, dev9_dma_done_callback cb, void *cb_arg)
{
	struct dev9_transfer *tr = &transfer;
	int dmactrl;

	M_DEBUG("%s\n", __func__);

	switch(ctrl){
	case 0:
	case 1:	dmactrl = ctrl;
		break;

	case 2:
	case 3:
		//if (dev9_predma_cbs[ctrl] == NULL)	return -1;
		//if (dev9_postdma_cbs[ctrl] == NULL)	return -1;
		dmactrl = (4 << ctrl);
		break;

	default:
		return -1;
	}

	tr->ctrl    = ctrl;
 	tr->dmactrl = dmactrl;
 	tr->addr    = addr;
	tr->bcr     = bcr;
	tr->dir     = dir;
	tr->cb      = cb;
	tr->cb_arg  = cb_arg;

	_dma_start(tr);

	return 0;
}

int
pata_ps2_dev9_init()
{
	M_DEBUG("%s\n", __func__);

	/* IOP<->DEV9 DMA completion interrupt */
	RegisterIntrHandler(IOP_IRQ_DMA_DEV9, 1, _dma_intr_handler, &transfer);
	EnableIntr(IOP_IRQ_DMA_DEV9);

	return 0;
}
