#include "pata_ps2.h"
#include "pata_ps2_ata.h"
#include "pata_ps2_buffer.h"


u8 _data_buffer[DATA_BUFFER_SIZE] __attribute((aligned(64)));
static u8 * _data_buffer_pointer = _data_buffer;
#define DATA_BUFFER_USED()	(_data_buffer_pointer-_data_buffer)
#define DATA_BUFFER_FREE()	(DATA_BUFFER_SIZE-DATA_BUFFER_USED())
#define DATA_BUFFER_RESET()	_data_buffer_pointer = _data_buffer

struct buffer_transfer {
	u32 size_left;		/* Size that still needs to be transferred */
	u32 size_xfer;		/* Size currently being transferred */
	block_done_callback cb;
	void *cb_arg;
};
static struct buffer_transfer transfer;

static void _transfer_start(struct buffer_transfer *tr);

/*
 * private functions
 */
static inline u32 min(u32 a, u32 b) {return (a<b)?a:b;}
static inline u32 max(u32 a, u32 b) {return (a>b)?a:b;}

static void
_transfer_callback(void *addr, u32 size, void *arg)
{
	struct buffer_transfer *tr = arg;

	M_DEBUG("(buf)%s(%lu, %lu)\n", __func__, addr, size);

	_data_buffer_pointer += size;
	tr->size_left -= size;

	/* NOTE: The callback can start a new transfer
	 *       invalidating our tr data.
	 */
	if (tr->size_left > 0) {
		if (tr->cb != NULL)
			tr->cb(addr, size, tr->cb_arg);
		_transfer_start(tr);
	}
	else {
		if (tr->cb != NULL)
			tr->cb(addr, size, tr->cb_arg);
	}
}

static void
_transfer_start(struct buffer_transfer *tr)
{
	M_DEBUG("(buf)%s\n", __func__);

	/* Limit the max transfer size */
	tr->size_xfer = min(tr->size_left, BUFFER_MAX_TRANSFER_SIZE);

	/* Reset the ring-buffer if needed */
	if (DATA_BUFFER_FREE() < tr->size_xfer)
		DATA_BUFFER_RESET();

	pata_ps2_ata_transfer(_data_buffer_pointer, tr->size_xfer, 0, _transfer_callback, tr);
}

/*
 * public functions
 */
void
pata_ps2_buffer_read(u32 size, block_done_callback cb, void *cb_arg)
{
	struct buffer_transfer *tr = &transfer;

	M_DEBUG("%s(%lu)\n", __func__, size);

	tr->size_left = size;
	tr->cb        = cb;
	tr->cb_arg    = cb_arg;

	_transfer_start(tr);
}

int
pata_ps2_buffer_init()
{
	M_DEBUG("%s\n", __func__);

	return 0;
}
