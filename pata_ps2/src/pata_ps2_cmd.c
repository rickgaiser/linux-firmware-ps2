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
	u32 offset;
	u32 size;
	u32 callback;
	u32 spare[16-4];
};

struct cmd_transfer {
	struct ps2_ata_cmd_rw cmd;
	u32 offset;		/* Address in EE where current transfer will go to */
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

	/* Start DMA transfer: IOP -> EE */
	if (tr->size_left > size) {
		/* Not finished, send data only */
		dma.src  = addr;
		dma.dest = (void *)tr->offset;
		dma.size = size;
		dma.attr = 0;
		/* FIXME: no isceSifSetDma? */
		sceSifSetDma(&dma, 1);

		/* Update statistics */
		tr->size_left -= size;
		tr->offset    += size;
	}
	else {
		/* Finished, send data and CMD */
		isceSifSendCmd(CMD_ATA_RW, (void *)&tr->cmd, sizeof(struct ps2_ata_cmd_rw), addr, (void *)tr->offset, size);
	}
}

static void
_cmd_handle(void *data, void *harg)
{
	struct ps2_ata_cmd_rw *cmd = (struct ps2_ata_cmd_rw *)data;
	struct cmd_transfer *tr = &transfer;

	M_DEBUG("cmd received (%lu, %lu, %lu)\n", cmd->write, cmd->offset, cmd->size);

	/* Set global state of transfer */
	tr->offset    = cmd->offset;
	tr->size_left = cmd->size;
	tr->cmd	      = *cmd;

	pata_ps2_buffer_transfer(cmd->size, cmd->write, _cmd_transfer_callback, tr);
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
