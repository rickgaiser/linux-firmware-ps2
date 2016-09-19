#include "pata_ps2.h"
#include "pata_ps2_dev9.h"
#include "pata_ps2_buffer.h"

#include "intrman.h"
#include "dmacman.h"

#include "dev9.h"
#include "speedregs.h"


u8 _data_buffer[DATA_BUFFER_SIZE] __attribute((aligned(64)));
static u8 * _data_buffer_pointer = _data_buffer;
#define DATA_BUFFER_USED()	(_data_buffer_pointer-_data_buffer)
#define DATA_BUFFER_FREE()	(DATA_BUFFER_SIZE-DATA_BUFFER_USED())
#define DATA_BUFFER_RESET()	_data_buffer_pointer = _data_buffer

enum{
	TRS_DONE=0,
	TRS_WAITING,	/* Waiting for data to/from ATA */
	TRS_TRANSFER	/* Transfer in progress */
};

struct buffer_transfer {
	void *addr;		/* Address in ring-buffer when writing */
	u32 size_left;		/* Size that still needs to be transferred */
	u32 size_xfer;		/* Size currently being transferred */
	int write;
	int state;
	block_done_callback cb;
	void *cb_arg;
};
static struct buffer_transfer transfer;

static void _transfer_sectors(struct buffer_transfer *tr);

/*
 * private functions
 */
static inline u32 min(u32 a, u32 b) {return (a<b)?a:b;}
static inline u32 max(u32 a, u32 b) {return (a>b)?a:b;}

#ifdef USE_PS2SDK_DEV9
static void
_transfer_callback(void *arg)
#else
static void
_transfer_callback(void *addr, u32 size, void *arg)
#endif
{
	struct buffer_transfer *tr = arg;
	void *addr2;

	if (tr->state != TRS_TRANSFER) {
		M_ERROR("not in transfer!\n");
		return;
	}

	addr2 = _data_buffer_pointer;
	_data_buffer_pointer += tr->size_xfer;
	tr->size_left -= tr->size_xfer;

	if (tr->size_left > 0) {
		tr->state = TRS_WAITING;
		tr->cb(addr2, tr->size_xfer, tr->cb_arg);
		_transfer_sectors(tr);
	}
	else {
		/* Done */
		tr->state = TRS_DONE;
		dev9LEDCtl(0);
		tr->cb(addr2, tr->size_xfer, tr->cb_arg);
	}
}

static void
_transfer_sectors(struct buffer_transfer *tr)
{
	if (tr->state != TRS_WAITING) {
		M_ERROR("not waiting!\n");
		return;
	}

	tr->size_xfer = tr->size_left;

	if (tr->write == 0) {
		/*
		 * When reading, the IOP controls the ring-buffer.
		 *
		 * Determine the optimal nr of sectors for transfer:
		 *   Bigger transfers between SPEED<->IOP are better, but
		 *   if we want the transfer from IOP->EE to run parallel
		 *   we need to split the transfer.
		 */
		if (tr->size_xfer > MIN_TRANSFER_SIZE) {
			/* Try to transfer 75% (aligned down), then start transfer to EE */
			tr->size_xfer = ((tr->size_xfer >> 2) * 3) & ~(512-1);
			/* But not too small */
			tr->size_xfer = max(tr->size_xfer, MIN_TRANSFER_SIZE);
			/* And not too big */
			tr->size_xfer = min(tr->size_xfer, MAX_TRANSFER_SIZE);
		}

		/* Reset the ring-buffer if needed */
		if (DATA_BUFFER_FREE() < tr->size_xfer)
			DATA_BUFFER_RESET();
	}

	/* Start DMA transfer: IOP <--> SPEED */
	tr->state = TRS_TRANSFER;
#ifdef USE_PS2SDK_DEV9
	dev9DmaTransferAsync(0, _data_buffer_pointer, (tr->size_xfer << 7)|128, (tr->write == 0) ? DMAC_TO_MEM : DMAC_FROM_MEM, _transfer_callback, tr);
#else
	pata_ps2_dev9_transfer(_data_buffer_pointer, tr->size_xfer, tr->write, _transfer_callback, tr);
#endif
}

#ifdef USE_PS2SDK_DEV9
static void AtadPreDmaCb(int bcr, int dir)
{
	USE_SPD_REGS;

	SPD_REG16(SPD_R_XFR_CTRL)|=0x80;
}

static void AtadPostDmaCb(int bcr, int dir)
{
	USE_SPD_REGS;

	SPD_REG16(SPD_R_XFR_CTRL)&=~0x80;
}
#endif

/*
 * public functions
 */
void
pata_ps2_buffer_transfer(void *addr, u32 size, u32 write, block_done_callback cb, void *cb_arg)
{
	struct buffer_transfer *tr = &transfer;

	if (tr->state != TRS_DONE) {
		M_ERROR("not done!\n");
		return;
	}

	tr->addr      = addr;
	tr->size_left = size;
	tr->write     = write;
	tr->cb        = cb;
	tr->cb_arg    = cb_arg;
	tr->state     = TRS_WAITING;

	/* When writing the writer controls where we start in the ring-buffer */
	if (write != 0)
		_data_buffer_pointer = addr;

	dev9LEDCtl(1);
	_transfer_sectors(tr);
}

int
pata_ps2_buffer_init()
{
	struct buffer_transfer *tr = &transfer;

	tr->state = TRS_DONE;

#ifdef USE_PS2SDK_DEV9
	dev9RegisterPreDmaCb (0, &AtadPreDmaCb);
	dev9RegisterPostDmaCb(0, &AtadPostDmaCb);
#endif

	return 0;
}
