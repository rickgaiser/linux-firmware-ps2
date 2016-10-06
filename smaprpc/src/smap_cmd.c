#include "main.h"
#include "smap.h"
#include "smap_cmd.h"
#include "pbuf.h"

#include "sifman.h"
#include "string.h"


u8 smap_tx_buffer[SMAP_TX_BUFFER_SIZE] __attribute((aligned(64)));
struct smap_soft_regs soft_regs;


static void
_cmd_transfer_callback(void *arg)
{
	struct pbuf *pBuf = (struct pbuf *)arg;
	struct ps2_smap_cmd_rw *cmd = &pBuf->cmd;

	//M_PRINTF("cmd done\n");

	/* Send CMD */
	if (cmd->callback == 1) {
		pBuf->id = isceSifSendCmd(CMD_SMAP_RW, cmd, sizeof(struct ps2_smap_cmd_rw) + cmd->sg_count * sizeof(struct ps2_smap_sg), NULL, NULL, 0);
		if (pBuf->id == 0) {
			M_ERROR("%s: isceSifSendCmd failed\n", __func__);
			pbuf_free(pBuf);
			return;
		}
	}
	else {
		pbuf_free(pBuf);
	}
}

static void
_cmd_handle(void *data, void *harg)
{
	struct ps2_smap_cmd_rw *cmd = data;
	struct pbuf *pBuf;
	int rv;

	//printf("smaprpc: cmd received (%lu, %lu, %lu)\n", cmd->write, cmd->addr, cmd->size);

	pBuf = pbuf_alloc(1, _cmd_transfer_callback, NULL, NULL);
	if (pBuf == NULL) {
		M_ERROR("Failed to allocate PBuf.\n");
		return;
	}

	/* Once we return from the CMD interrupt, the data will be invalid, so make a copy. */
	memcpy(&pBuf->cmd, cmd, sizeof(struct ps2_smap_cmd_rw) + cmd->sg_count * sizeof(struct ps2_smap_sg));

	pBuf->len = pBuf->cmd_sg[0].size;
	pBuf->payload = (void *)pBuf->cmd_sg[0].addr;
	pBuf->cb_arg = pBuf;

	rv = SMapLowLevelOutput(pBuf);
	if (rv != 0) {
		M_ERROR("SMapLowLevelOutput returned %d\n", rv);
		return;
	}
}

static void
_cmd_handle_reset(void *data, void *harg)
{
	smap_reset();
}

/*
 * public functions
 */
int
smap_cmd_init()
{
	M_DEBUG("%s\n", __func__);

	soft_regs.tx_buffer_addr = (u32)smap_tx_buffer;
	soft_regs.tx_buffer_size = SMAP_TX_BUFFER_SIZE;

	sceSifAddCmdHandler(CMD_SMAP_RW, _cmd_handle, NULL);
	sceSifAddCmdHandler(CMD_SMAP_RESET, _cmd_handle_reset, NULL);

	return 0;
}
