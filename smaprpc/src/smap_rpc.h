#ifndef _SMAP_RPC_H_
#define _SMAP_RPC_H_


#include <sifrpc.h>


/* RPC from EE -> IOP */
#define SMAP_BIND_RPC_ID	0x0815e000
//#define SMAP_CMD_SEND		1
#define SMAP_CMD_SET_BUFFER	2
#define SMAP_CMD_GET_MAC_ADDR	3
#define SMAP_CMD_SET_MAC_ADDR	4
#define SMAP_CMD_MDIO_READ	5
#define SMAP_CMD_MDIO_WRITE	6
#define SMAP_CMD_SET_LNK	7


int smap_rpc_init();


#endif
