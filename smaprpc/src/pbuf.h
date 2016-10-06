#ifndef _PBUF_H_
#define _PBUF_H_


#include "types.h"
#include "smap_cmd.h"
#include "smap_dma.h"


typedef void (*block_done_callback)(void *arg);

typedef struct pbuf {
	/** pointer to the actual data in the buffer */
	void *payload;

	/* length of this buffer */
	u16 len;

	/**
	* the reference count always equals the number of pointers
	* that refer to this pbuf.
	*/
	u16 ref;

	/** Transfer id of received frame which is sent to the EE. */
	int id;

	struct ps2_smap_cmd_rw cmd __attribute__((aligned(64)));
	struct ps2_smap_sg cmd_sg[MAX_SG_COUNT];

	struct smap_dma_transfer smaptr[1];

	block_done_callback cb;
	void *cb_arg;
} PBuf;

typedef int err_t;

u8 pbuf_free(struct pbuf *p);
void pbuf_reset(void);
struct pbuf *pbuf_alloc(u16 size, block_done_callback cb, void *cb_arg, u8 * buffer);
void pbuf_check_transfers(void);

#endif
