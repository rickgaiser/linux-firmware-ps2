#ifndef PATA_PS2_H
#define PATA_PS2_H


#include "types.h"
#include "stdio.h"


#define MODNAME			"pata_ps2"

#define M_PRINTF(format, args...)	\
	printf(MODNAME ": " format, ## args)

#define M_ERROR(format, args...) M_PRINTF("ERROR: " format, ## args)

#if 0
#define M_DEBUG M_PRINTF
#else
#define M_DEBUG(format, args...)
#endif

#define ATA_DIR_READ		0
#define ATA_DIR_WRITE		1


#endif
