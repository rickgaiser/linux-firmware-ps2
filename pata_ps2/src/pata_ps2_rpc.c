#include "pata_ps2.h"
#include "pata_ps2_core.h"
#include "pata_ps2_buffer.h"
#include "pata_ps2_rpc.h"

#include "thbase.h"
#include "sifman.h"
#include "sifcmd.h"
#include "speedregs.h"


#define PATA_PS2_IRX		0xAAAABBBB
#define PATA_PS2_GET_ADDR	3
#define PATA_PS2_SET_DIR	4

struct ps2_ata_rpc_get_addr {
	u32 ret;
	u32 addr;
	u32 size;
};

struct ps2_ata_rpc_set_dir {
	u32 dir;
};


static SifRpcDataQueue_t rpc_queue __attribute__((aligned(64)));
static SifRpcServerData_t rpc_server __attribute((aligned(64)));
static u8 _rpc_buffer[1024] __attribute((aligned(64)));


static int pata_ps2_rpc_get_addr(struct ps2_ata_rpc_get_addr *rpc)
{
	rpc->addr = (u32)_data_buffer;
	rpc->size = DATA_BUFFER_SIZE;

	return 0;
}

static void *pata_ps2_rpc_cmd_handler(u32 command, void *buffer, int size)
{
	int ret;

	switch (command) {
		case PATA_PS2_GET_ADDR:
			M_DEBUG("PATA_PS2_GET_ADDR()\n");
			ret = pata_ps2_rpc_get_addr((struct ps2_ata_rpc_get_addr *)buffer);
			break;

		case PATA_PS2_SET_DIR:
			M_DEBUG("PATA_PS2_SET_DIR(%lu)\n", ((u32 *)buffer)[0]);
			pata_ps2_core_set_dir(((u32 *)buffer)[0]);
			ret = 0;
			break;

		default:
			M_ERROR("unknown cmd %lu\n", command);
			ret = -1;
			break;
	}

	((u32 *)buffer)[0] = ret;

	return buffer;
}

static void pata_ps2_rpc_thread(void* param)
{
	int tid;

	M_PRINTF("RPC thread running\n");

	tid = GetThreadId();

	sceSifSetRpcQueue(&rpc_queue, tid);
	sceSifRegisterRpc(&rpc_server, PATA_PS2_IRX, (void *)pata_ps2_rpc_cmd_handler, _rpc_buffer, 0, 0, &rpc_queue);
	sceSifRpcLoop(&rpc_queue);
}

int pata_ps2_rpc_init()
{
	iop_thread_t param;
	int th;

	/*create thread*/
	param.attr	= TH_C;
	param.thread	= pata_ps2_rpc_thread;
	param.priority	= 40;
	param.stacksize	= 0x1000;
	param.option	= 0;

	th = CreateThread(&param);
	if (th > 0) {
		StartThread(th, 0);
		return 0;
	} else
		return 1;
}

