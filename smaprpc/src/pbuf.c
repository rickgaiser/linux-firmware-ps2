#include "pbuf.h"
#include "main.h"
#include "sifman.h"


#define NUMBER_OF_BUFFERS 64
static struct pbuf buffers[NUMBER_OF_BUFFERS] __attribute__((aligned(64)));
//static int read_idx = 0;
static int write_idx = 0;
#define BUFFER_NEXT(idx) ((idx+1)&(NUMBER_OF_BUFFERS-1))

void
pbuf_check_transfers(void)
{
	int i;

	for (i = 0; i < NUMBER_OF_BUFFERS; i++) {
		if ((buffers[i].ref != 0) && (buffers[i].id != 0)) {
			if (SifDmaStat(buffers[i].id) < 0) {
				/* DMA transfer is complete. */
				pbuf_free(&buffers[i]);
			}
		}
	}
}

u8
pbuf_free(struct pbuf *p)
{
	p->id = 0;
	p->ref = 0;
	return 1;
}

void
pbuf_reset(void)
{
	int i;
	int count = 0;

	for (i = 0; i < NUMBER_OF_BUFFERS; i++) {
		if (buffers[i].ref != 0)
			count++;

		pbuf_free(&buffers[i]);
	}

	M_DEBUG("%s: count = %d\n", __func__, count);
}

struct pbuf *
pbuf_alloc(u16 size, block_done_callback cb, void *cb_arg, u8 * buffer)
{
	int i = write_idx;

	pbuf_check_transfers();
	if (buffers[i].ref == 0) {
		write_idx = BUFFER_NEXT(write_idx);
		buffers[i].ref = 1;
		buffers[i].payload = buffer;
		buffers[i].len = size;
		buffers[i].id = 0;
		buffers[i].cb = cb;
		buffers[i].cb_arg = cb_arg;
		return &buffers[i];
	}

	return NULL;
}
