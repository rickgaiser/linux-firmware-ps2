#include <tamtypes.h>
#include <thbase.h>
#include <thsemap.h>
#include <sifrpc.h>
#include <ioman.h>
#include <sysclib.h>
#include <sifman.h>

#include "stddef.h"
#include "stdio.h"
#include "smap.h"
#include "smap_cmd.h"
#include "smap_rpc.h"
#include "smap_mdio.h"
#include "main.h"
#include "pbuf.h"


static SifRpcDataQueue_t rpc_queue __attribute__((aligned(64)));
static SifRpcServerData_t rpc_server __attribute((aligned(64)));
static u8 _rpc_buffer[1024] __attribute((aligned(64)));


/*
 * private functions
 */
static void *
_rpc_cmd_handler(u32 command, void *buffer, int size)

{
	u32 *buf = (u32 *)buffer;
	int ret = 0;

	switch (command) {
		case SMAP_CMD_SET_BUFFER: {
			unsigned int offset;

			ee_buffer_size = 0; /* Disable use of ee_buffer in case an interrupt occurs. */
			ee_buffer = buf[0];
			if (buf[1] > 0) {
				/* Align memory offset. */
				offset = DMA_TRANSFER_SIZE - (ee_buffer & (DMA_TRANSFER_SIZE - 1));
				if (offset == DMA_TRANSFER_SIZE) {
					offset = 0;
				}
				ee_buffer = ee_buffer + offset;
				ee_buffer_pos = 0;
				ee_buffer_size = buf[1] - offset;
			}
			buf[1] = (u32)(&soft_regs);
			break;
		}

		case SMAP_CMD_GET_MAC_ADDR: {
			memcpy(&buf[1], SMap_GetMACAddress(), 6);
			break;
		}

		case SMAP_CMD_SET_MAC_ADDR: {
			SMap_SetMacAddress((const char *)buffer);
			break;
		}

		case SMAP_CMD_MDIO_READ: {
			ret = smap_mdio_read(&SMap0, buf[0], buf[1]);
			break;
		}

		case SMAP_CMD_MDIO_WRITE: {
			ret = smap_mdio_write(&SMap0, buf[0], buf[1], buf[2]);
			break;
		}

		case SMAP_CMD_SET_LNK: {
			SMap_SetLink(&SMap0, buf[0], buf[1], buf[2]);
			break;
		}

		default:
			M_ERROR("unknown cmd %lu\n", command);
			ret = -1;
			break;
	}

	buf[0] = ret;

	return buffer;
}

static void
_rpc_thread(void* param)
{
	int tid;

	M_DEBUG("%s\n", __func__);
	M_PRINTF("RPC thread running\n");

	tid = GetThreadId();

	sceSifSetRpcQueue(&rpc_queue, tid);
	sceSifRegisterRpc(&rpc_server, SMAP_BIND_RPC_ID, (void *)_rpc_cmd_handler, _rpc_buffer, 0, 0, &rpc_queue);
	sceSifRpcLoop(&rpc_queue);
}

/*
 * public functions
 */
int
smap_rpc_init()
{
	iop_thread_t param;
	int th;

	/*create thread*/
	param.attr      = TH_C;
	param.thread    = _rpc_thread;
	param.priority  = 40;
	param.stacksize = 0x1000;
	param.option    = 0;

	th = CreateThread(&param);
	if (th > 0) {
		StartThread(th, 0);
		return 0;
	} else
		return 1;
}
