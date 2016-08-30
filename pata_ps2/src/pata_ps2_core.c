#include "pata_ps2.h"
#include "pata_ps2_core.h"

#include "intrman.h"
#include "dmacman.h"

#include "dev9regs.h"
#include "speedregs.h"


#ifndef USE_PS2SDK_DEV9
struct core_transfer {
	void * addr;
	u32 size;
	int dir;
	block_done_callback cb;
	void *cb_arg;
};
static struct core_transfer transfer;


/*
 * private functions
 */
static inline void
_dma_start(struct core_transfer *tr)
{
	USE_SPD_REGS;
	volatile iop_dmac_chan_t *dev9_chan = (iop_dmac_chan_t *)DEV9_DMAC_BASE;

	M_DEBUG("DEV9_DMA start (%lub)\n", tr->size);

	/* Start DMA transfer: IOP <--> SPEED */
	SPD_REG16(SPD_R_XFR_CTRL)|=0x80;
	SPD_REG16(SPD_R_DMA_CTRL) = (SPD_REG16(SPD_R_REV_1)<17)?0x04:0x06;
	dev9_chan->madr = (u32)tr->addr;
	dev9_chan->bcr  = (tr->size << 9)|32;
	dev9_chan->chcr = DMAC_CHCR_30|DMAC_CHCR_TR|DMAC_CHCR_CO|(tr->dir & DMAC_CHCR_DR);
}

static inline void
_dma_stop()
{
	USE_SPD_REGS;

	SPD_REG16(SPD_R_XFR_CTRL)&=~0x80;
}

static int
_dma_intr_handler(void *arg)
{
	struct core_transfer *tr = arg;

	M_DEBUG("DEV9_DMA compl (%lub)\n", tr->size);

	_dma_stop();

	tr->cb(tr->addr, tr->size, tr->cb_arg);

	return 1;
}
#endif

/*
 * public functions
 */
void
pata_ps2_core_set_dir(int dir)
{
	USE_SPD_REGS;
	u16 val;

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

#ifndef USE_PS2SDK_DEV9
void
pata_ps2_core_transfer(void *addr, u32 size, u32 write, block_done_callback cb, void *cb_arg)
{
	struct core_transfer *tr = &transfer;

	tr->addr   = addr;
	tr->size   = size;
	tr->dir    = (write == 0) ? DMAC_TO_MEM : DMAC_FROM_MEM;
	tr->cb     = cb;
	tr->cb_arg = cb_arg;

	_dma_start(tr);
}
#endif

int
pata_ps2_core_init()
{
#ifndef USE_PS2SDK_DEV9
	/* IOP<->DEV9 DMA completion interrupt */
	RegisterIntrHandler(IOP_IRQ_DMA_DEV9, 1, _dma_intr_handler, &transfer);
	EnableIntr(IOP_IRQ_DMA_DEV9);
#endif

	return 0;
}
