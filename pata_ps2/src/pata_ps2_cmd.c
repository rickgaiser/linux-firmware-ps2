#include "types.h"

#include "sifman.h"
#include "sifcmd.h"
#include "string.h"

#include "pata_ps2.h"
#include "pata_ps2_buffer.h"
#include "pata_ps2_ata.h"
#include "pata_ps2_cmd.h"


#define CMD_ATA_RW		0x18 // is this CMD number free?
struct ps2_ata_sg { /* 8 bytes */
	u32 addr;
	u32 size;
};

/* Commands need to be 16byte aligned */
struct ps2_ata_cmd_rw {
	/* Header: 16 bytes */
	struct t_SifCmdHeader sifcmd;
	/* Data: 8 bytes */
	u32 write:1;
	u32 callback:1;
	u32 sg_count:30;
	u32 _spare;
};
#define MAX_CMD_SIZE (112)
#define CMD_BUFFER_SIZE (MAX_CMD_SIZE)
#define MAX_SG_COUNT ((CMD_BUFFER_SIZE - sizeof(struct ps2_ata_cmd_rw)) / sizeof(struct ps2_ata_sg))

struct cmd_transfer {
	struct ps2_ata_cmd_rw cmd;
	struct ps2_ata_sg cmd_sg[MAX_SG_COUNT];
	void *addr;		/* Address in EE where current transfer will go to */
	u32 size_left;		/* Size that still needs to be transferred */
	u32 sg_index;
};
static struct cmd_transfer transfer __attribute((aligned(64)));


/*
 * private functions
 */
static void _transfer_callback(void *addr, u32 size, void *arg);

static void
_start_sg(struct cmd_transfer *tr)
{
	struct ps2_ata_sg *sg;

	M_DEBUG("%s\n", __func__);

	if (tr->sg_index < tr->cmd.sg_count) {
		/* Not finished */

		/* start next sg */
		sg = &tr->cmd_sg[tr->sg_index];

		M_DEBUG("start sg (%d, %lu, %lu, %lu)\n", tr->cmd.write, tr->sg_index, sg->addr, sg->size);

		tr->addr       = (void *)sg->addr;
		tr->size_left  = sg->size;

		if (tr->cmd.write == 0) {
			/* Read using ring-buffer */
			pata_ps2_buffer_read(sg->size, _transfer_callback, tr);
		}
		else {
			/* Write directly */
			pata_ps2_ata_transfer((void *)sg->addr, sg->size, tr->cmd.write, _transfer_callback, tr);
		}
	}
	else {
		/* Finished */

		/* Send CMD */
		isceSifSendCmd(CMD_ATA_RW, (void *)&tr->cmd, sizeof(struct ps2_ata_cmd_rw), NULL, NULL, 0);
	}
}

static void
_transfer_callback(void *addr, u32 size, void *arg)
{
	struct cmd_transfer *tr = arg;
#ifndef SPEED_TEST
	SifDmaTransfer_t dma;

	M_DEBUG("(cmd)%s(%lu, %lu)\n", __func__, addr, size);

	if (tr->cmd.write == 0) {
		/* Send data only */
		dma.src  = addr;
		dma.dest = tr->addr;
		dma.size = size;
		dma.attr = 0;
		/* FIXME: no isceSifSetDma? */
		sceSifSetDma(&dma, 1);
	}
#else
	M_DEBUG("(cmd)%s(%lu, %lu)\n", __func__, addr, size);
#endif

	if (tr->size_left > size) {
		/* Not finished */

		/* Update statistics */
		tr->addr       = (u8 *)tr->addr + size;
		tr->size_left -= size;
	}
	else {
		/* Finished */

		tr->sg_index++;
		_start_sg(tr);
	}
}

static void
_cmd_handle(void *data, void *harg)
{
	struct ps2_ata_cmd_rw *cmd = (struct ps2_ata_cmd_rw *)data;
	struct cmd_transfer *tr = &transfer;

	M_DEBUG("%s(sg_count = %d)\n", __func__, cmd->sg_count);

	/* Once we return from the CMD interrupt, the data will be invalid, so make a copy. */
	memcpy(&tr->cmd, cmd, sizeof(struct ps2_ata_cmd_rw) + cmd->sg_count * sizeof(struct ps2_ata_sg));

	tr->sg_index = 0;
	_start_sg(tr);
}

/*
 * public functions
 */
int
pata_ps2_cmd_init()
{
	M_DEBUG("%s\n", __func__);

	sceSifAddCmdHandler(CMD_ATA_RW, _cmd_handle, NULL);

	return 0;
}
