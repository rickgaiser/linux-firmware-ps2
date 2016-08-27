#include "pata_ps2.h"
#include "pata_ps2_core.h"
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
	u32 size_left;		/* Size that still needs to be transferred */
	u32 size_xfer;		/* Size currently being transferred */
	int write;
	int state;
	block_done_callback cb;
	void *cb_arg;
};
static struct buffer_transfer transfer;

static void _transfer_block(struct buffer_transfer *tr);

/*
 * private functions
 */
static inline u32 min(u32 a, u32 b) {return (a<b)?a:b;}
static inline u32 max(u32 a, u32 b) {return (a>b)?a:b;}

static void
_transfer_callback(void *addr, u32 size, void *arg)
{
	struct buffer_transfer *tr = arg;

	if (tr->state != TRS_TRANSFER) {
		M_ERROR("not in transfer!\n");
		return;
	}

	tr->cb(addr, size, tr->cb_arg);

	_data_buffer_pointer += tr->size_xfer;
	tr->size_left -= tr->size_xfer;
	if (tr->size_left > 0) {
		/* Advance to next transfer */
		tr->state = TRS_WAITING;
		_transfer_block(tr);
	}
	else {
		/* Done */
		tr->state = TRS_DONE;
		dev9LEDCtl(0);
	}
}

static void
_transfer_block(struct buffer_transfer *tr)
{
	if (tr->state != TRS_WAITING) {
		M_ERROR("not waiting!\n");
		return;
	}

	/*
	 * Determine the optimal nr of blocks for transfer:
	 *   Bigger transfers between SPEED<->IOP are better, but
	 *   if we want the transfer from IOP->EE to run parallel
	 *   we need to split the transfer.
	 */
	tr->size_xfer = tr->size_left;
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

	/* Start DMA transfer: IOP <--> SPEED */
	tr->state = TRS_TRANSFER;
	pata_ps2_core_transfer(_data_buffer_pointer, tr->size_xfer, tr->write, _transfer_callback, tr);
}

/*
 * public functions
 */
void
pata_ps2_buffer_transfer(u32 size, u32 write, block_done_callback cb, void *cb_arg)
{
	struct buffer_transfer *tr = &transfer;

	if (tr->state != TRS_DONE) {
		M_ERROR("not done!\n");
		return;
	}

	tr->size_left = size;
	tr->write     = write;
	tr->cb        = cb;
	tr->cb_arg    = cb_arg;
	tr->state     = TRS_WAITING;

	dev9LEDCtl(1);
	_transfer_block(tr);
}

int
pata_ps2_buffer_init()
{
	struct buffer_transfer *tr = &transfer;

	tr->state = TRS_DONE;

	return 0;
}
