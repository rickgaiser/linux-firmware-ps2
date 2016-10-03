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
#define DEV9_DMA_QUEUE_SIZE 32
static struct dev9_transfer tqueue[DEV9_DMA_QUEUE_SIZE];
static int tqueue_write_idx = 0;
static int tqueue_read_idx = 0;
#define QUEUE_NEXT(idx)	((idx + 1) & (DEV9_DMA_QUEUE_SIZE-1))
#define QUEUE_EMPTY()	(tqueue_write_idx == tqueue_read_idx)
#define QUEUE_FULL()	(QUEUE_NEXT(tqueue_write_idx) == tqueue_read_idx)


/*
 * private functions
 */
static inline void
_dma_start(struct dev9_transfer *tr)
{
	USE_SPD_REGS;
	volatile iop_dmac_chan_t *dev9_chan = (iop_dmac_chan_t *)DEV9_DMAC_BASE;

	M_DEBUG("%s\n", __func__);

	SPD_REG16(SPD_R_DMA_CTRL) = (SPD_REG16(SPD_R_REV_1)<17)?(tr->dmactrl&0x03)|0x04:(tr->dmactrl&0x01)|0x06;

	SPD_REG16(SPD_R_XFR_CTRL)|=0x80;

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
	struct dev9_transfer *tr = &tqueue[tqueue_read_idx];

	M_DEBUG("%s\n", __func__);

	_dma_stop();

	/* Remove transfer from queue */
	/* NOTE: Data stays valid until we return from this interrupt */
	tqueue_read_idx = QUEUE_NEXT(tqueue_read_idx);

	/* Start next DMA transfer if queue not empty */
	if (!QUEUE_EMPTY())
		_dma_start(&tqueue[tqueue_read_idx]);

	/* Call DMA done callback */
	if (tr->cb != NULL)
		tr->cb(tr->cb_arg);

	return 1;
}

/*
 * public functions
 */
int
pata_ps2_dev9_transfer(int ctrl, void *addr, int bcr, int dir, dev9_dma_done_callback cb, void *cb_arg)
{
	struct dev9_transfer *tr;
	int dmactrl, flags, dma_stopped;

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

	CpuSuspendIntr(&flags);

	/* Get unused transfer from queue */
	if (QUEUE_FULL()) {
		CpuResumeIntr(flags);
		return -1;
	}
	dma_stopped = QUEUE_EMPTY() ? 1 : 0;
	tr = &tqueue[tqueue_write_idx];
	tqueue_write_idx = QUEUE_NEXT(tqueue_write_idx);

	tr->ctrl    = ctrl;
 	tr->dmactrl = dmactrl;
 	tr->addr    = addr;
	tr->bcr     = bcr;
	tr->dir     = dir;
	tr->cb      = cb;
	tr->cb_arg  = cb_arg;

	/* Start the DMA engine if it was stopped */
	if (dma_stopped == 1)
		_dma_start(tr);

	CpuResumeIntr(flags);

	return 0;
}

int
pata_ps2_dev9_init()
{
	M_DEBUG("%s\n", __func__);

	/* IOP<->DEV9 DMA completion interrupt */
	RegisterIntrHandler(IOP_IRQ_DMA_DEV9, 1, _dma_intr_handler, NULL);
	EnableIntr(IOP_IRQ_DMA_DEV9);

	return 0;
}
