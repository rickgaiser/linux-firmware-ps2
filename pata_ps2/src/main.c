#include "irx.h"
#include "loadcore.h"

#include "sifman.h"
#include "sifcmd.h"

#include "pata_ps2.h"
#include "pata_ps2_ata.h"
#include "pata_ps2_buffer.h"
#include "pata_ps2_cmd.h"
#include "pata_ps2_rpc.h"


IRX_ID(MODNAME, 1, 0);


int
_start(int argc, char *argv[])
{
	if (!sceSifCheckInit())
		sceSifInit();
	sceSifInitRpc(0);

	if(pata_ps2_ata_init() != 0) {
		M_ERROR("failed to init ata\n");
		return MODULE_NO_RESIDENT_END;
	}

	if(pata_ps2_buffer_init() != 0) {
		M_ERROR("failed to init buffer\n");
		return MODULE_NO_RESIDENT_END;
	}

	if(pata_ps2_cmd_init() != 0) {
		M_ERROR("failed to init cmd\n");
		return MODULE_NO_RESIDENT_END;
	}

	if(pata_ps2_rpc_init() != 0) {
		M_ERROR("failed to init rpc\n");
		return MODULE_NO_RESIDENT_END;
	}

	M_PRINTF("running\n");

	return MODULE_RESIDENT_END;
}
