# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004.
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_OBJS_DIR = obj/
IOP_BIN_DIR = bin/
IOP_SRC_DIR = src/

IOP_BIN  = $(IOP_BIN_DIR)pata_ps2.irx
IOP_MODS = main pata_ps2_dev9 pata_ps2_ata pata_ps2_buffer pata_ps2_cmd pata_ps2_rpc imports
IOP_OBJS = $(addsuffix .o, $(addprefix $(IOP_OBJS_DIR), $(IOP_MODS)))
IOP_INCS = -I$(PS2SDKSRC)/iop/dev9/dev9/include

all: $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release
