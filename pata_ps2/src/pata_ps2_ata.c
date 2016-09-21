#include "pata_ps2.h"
#include "pata_ps2_dev9.h"
#include "pata_ps2_ata.h"

#include "intrman.h"
#include "dmacman.h"

#include "dev9.h"
#include "speedregs.h"


enum{
	TRS_INVALID=0,
	TRS_DONE,
	TRS_WAITING,		/* Waiting for data to/from ATA */
	TRS_TRANSFER		/* Transfer in progress */
};

struct ata_transfer {
	void *addr;		/* Memory address */
	u32 size_left;		/* Size that still needs to be transferred */
	u32 size_xfer;		/* Size currently being transferred */
	int write;
	int state;
	block_done_callback cb;
	void *cb_arg;
};
static struct ata_transfer transfer;

static void _transfer_start(struct ata_transfer *tr);

/*
 * private functions
 */
static inline u32 min(u32 a, u32 b) {return (a<b)?a:b;}
static inline u32 max(u32 a, u32 b) {return (a>b)?a:b;}

static void
_transfer_callback(void *arg)
{
	struct ata_transfer *tr = arg;
	void *addr2;

	M_DEBUG("(ata)%s(%lu, %lu)\n", __func__, tr->addr, tr->size_xfer);

	if (tr->state != TRS_TRANSFER) {
		M_ERROR("%d != TRS_TRANSFER\n", tr->state);
		return;
	}

	addr2 = tr->addr;
	tr->addr = (u8 *)tr->addr + tr->size_xfer;
	tr->size_left -= tr->size_xfer;

	/* NOTE: The callback can start a new transfer
	 *       invalidating our tr data.
	 */
	if (tr->size_left > 0) {
		tr->state = TRS_WAITING;
		if (tr->cb != NULL)
			tr->cb(addr2, tr->size_xfer, tr->cb_arg);
		_transfer_start(tr);
	}
	else {
		/* Done */
		tr->state = TRS_DONE;
		dev9LEDCtl(0);
		if (tr->cb != NULL)
			tr->cb(addr2, tr->size_xfer, tr->cb_arg);
	}
}

static void
_transfer_start(struct ata_transfer *tr)
{
	M_DEBUG("(ata)%s\n", __func__);

	if (tr->state != TRS_WAITING) {
		M_ERROR("%d != TRS_WAITING\n", tr->state);
		return;
	}

	tr->size_xfer = min(tr->size_left, ATA_MAX_TRANSFER_SIZE);

	/* Start DMA transfer: IOP <--> SPEED */
	tr->state = TRS_TRANSFER;
	pata_ps2_dev9_transfer(0, tr->addr, (tr->size_xfer << 7)|128, (tr->write == 0) ? DMAC_TO_MEM : DMAC_FROM_MEM, _transfer_callback, tr);
}

#ifdef USE_PS2SDK_DEV9
static void AtadPreDmaCb(int bcr, int dir)
{
	USE_SPD_REGS;

	M_DEBUG("%s\n", __func__);

	SPD_REG16(SPD_R_XFR_CTRL)|=0x80;
}

static void AtadPostDmaCb(int bcr, int dir)
{
	USE_SPD_REGS;

	M_DEBUG("%s\n", __func__);

	SPD_REG16(SPD_R_XFR_CTRL)&=~0x80;
}
#endif

/* ATA command completion interrupt */
static int _intr_ata0(int flag)
{
	struct ata_transfer *tr = &transfer;

	dev9IntrDisable(SPD_INTR_ATA0);

	/* When writing, the completion interrupt should happen after we are
	 * done writing data to SPEED
	 */
	if (tr->state != TRS_DONE) {
		M_ERROR("%d != TRS_DONE", tr->state);
		return 1;
	}

	if (tr->cb != NULL) {
		/* FIXME: NULL means ATA completion interrupt */
		tr->cb(NULL, 0, tr->cb_arg);
	}

	return 1;
}

/* ATA ??? ready for DMA ??? interrupt */
static int _intr_ata1(int flag)
{
	dev9IntrDisable(SPD_INTR_ATA1);

	return 1;
}

/*
 * public functions
 */
void
pata_ps2_ata_transfer(void *addr, u32 size, u32 write, block_done_callback cb, void *cb_arg)
{
	struct ata_transfer *tr = &transfer;

	M_DEBUG("%s(%lu)\n", __func__, size);

	if (tr->state != TRS_DONE) {
		M_ERROR("%d != TRS_DONE\n", tr->state);
		return;
	}

	tr->addr      = addr;
	tr->size_left = size;
	tr->write     = write;
	tr->cb        = cb;
	tr->cb_arg    = cb_arg;
	tr->state     = TRS_WAITING;

	/* Enable the ATA completion interrupt when writing */
	if (tr->write != 0)
		dev9IntrEnable(SPD_INTR_ATA0);

	dev9LEDCtl(1);
	_transfer_start(tr);
}

int
pata_ps2_ata_init()
{
	struct ata_transfer *tr = &transfer;

	M_DEBUG("%s\n", __func__);

	tr->state = TRS_DONE;

#ifdef USE_PS2SDK_DEV9
	dev9RegisterPreDmaCb (0, &AtadPreDmaCb);
	dev9RegisterPostDmaCb(0, &AtadPostDmaCb);
#endif

	dev9IntrDisable(SPD_INTR_ATA0|SPD_INTR_ATA1);
	dev9RegisterIntrCb(0, &_intr_ata0);
	dev9RegisterIntrCb(1, &_intr_ata1);

	return 0;
}
