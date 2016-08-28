#include "types.h"

#include "sifman.h"
#include "sifcmd.h"

#include "pata_ps2.h"
#include "pata_ps2_core.h"
#include "pata_ps2_buffer.h"
#include "pata_ps2_cmd.h"


#define CMD_ATA_RW 0x18 // is this CMD number free?
struct ps2_ata_cmd_rw {
	struct t_SifCmdHeader sifcmd;
	u32 write;
	u32 addr;
	u32 size;
	u32 callback;
	u32 spare[16-4];
};

struct cmd_transfer {
	struct ps2_ata_cmd_rw cmd;
	void *addr;		/* Address in EE where current transfer will go to */
	u32 size_left;		/* Size that still needs to be transferred */
};
static struct cmd_transfer transfer __attribute((aligned(64)));


/*
 * private functions
 */
static void
_cmd_transfer_callback(void *addr, u32 size, void *arg)
{
	struct cmd_transfer *tr = arg;
	SifDmaTransfer_t dma;

	if (tr->size_left > size) {
		/* Not finished */

		if (tr->cmd.write == 0) {
			/* Send data only */
			dma.src  = addr;
			dma.dest = tr->addr;
			dma.size = size;
			dma.attr = 0;
			/* FIXME: no isceSifSetDma? */
			sceSifSetDma(&dma, 1);
		}

		/* Update statistics */
		tr->size_left -= size;
		tr->addr       = (u8 *)tr->addr + size;
	}
	else {
		/* Finished */
		if (tr->cmd.write == 0) {
			/* Send CMD and data */
			isceSifSendCmd(CMD_ATA_RW, (void *)&tr->cmd, sizeof(struct ps2_ata_cmd_rw), addr, tr->addr, size);
		}
		else {
			/* Send CMD */
			isceSifSendCmd(CMD_ATA_RW, (void *)&tr->cmd, sizeof(struct ps2_ata_cmd_rw), NULL, NULL, 0);
		}
	}
}

static void
_cmd_handle(void *data, void *harg)
{
	struct ps2_ata_cmd_rw *cmd = (struct ps2_ata_cmd_rw *)data;
	struct cmd_transfer *tr = &transfer;

	M_DEBUG("cmd received (%lu, %lu, %lu)\n", cmd->write, cmd->addr, cmd->size);

	/* Set global state of transfer */
	tr->addr      = (void *)cmd->addr;
	tr->size_left = cmd->size;
	tr->cmd	      = *cmd;

	pata_ps2_buffer_transfer((void *)cmd->addr, cmd->size, cmd->write, _cmd_transfer_callback, tr);
}

/*
 * public functions
 */
int
pata_ps2_cmd_init()
{
	sceSifAddCmdHandler(CMD_ATA_RW, _cmd_handle, NULL);

	return 0;
}
