#ifndef SMAP_CMD_H
#define SMAP_CMD_H


#include "sifcmd.h"


/* CMD from IOP -> EE */
#define SIF_SMAP_RECEIVE	0x07

/* CMD from EE -> IOP */
#define CMD_SMAP_RW		0x17 // is this CMD number free?
#define CMD_SMAP_RESET		0x19 // is this CMD number free?

struct smap_soft_regs {
	u32 tx_buffer_addr;
	u32 tx_buffer_size;
	u32 _spare1;
	u32 _spare2;
};

struct ps2_smap_sg { /* 8 bytes */
	u32 addr;
	u32 size;
};

/* Commands need to be 16byte aligned */
struct ps2_smap_cmd_rw {
	/* Header: 16 bytes */
	struct t_SifCmdHeader sifcmd;
	/* Data: 8 bytes */
	u32 write:1;
	u32 callback:1;
	u32 ata0_intr:1;
	u32 sg_count:29;
	u32 _spare;
};
#define MAX_CMD_SIZE (112)
#define CMD_BUFFER_SIZE (MAX_CMD_SIZE)
#define MAX_SG_COUNT ((CMD_BUFFER_SIZE - sizeof(struct ps2_smap_cmd_rw)) / sizeof(struct ps2_smap_sg))

#define SMAP_TX_BUFFER_SIZE (32*1024)
#define SMAP_RX_BUFFER_SIZE (32*1024)
extern u8 smap_tx_buffer[SMAP_TX_BUFFER_SIZE];
extern u8 smap_rx_buffer[SMAP_RX_BUFFER_SIZE];
extern struct smap_soft_regs soft_regs;

int smap_cmd_init();


#endif
