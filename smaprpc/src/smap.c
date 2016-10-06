/*
   smap.c

   Copyright (c)2001 Sony Computer Entertainment Inc.
   Copyright (c)2001 YAEGASHI Takeshi
   Copyright (2)2002 Dan Potter
   Copyright (c)2003 T Lindstrom
   Copyright (c)2003 adresd
   License: GPL

   $Id: smap.c,v 1.1 2009/07/26 17:56:20 kloader Exp $
*/

/* Pulled from the eCos port */

// $Id: smap.c,v 1.1 2009/07/26 17:56:20 kloader Exp $
// many routines are taken from drivers/ps2/smap.c of PS2 Linux kernel (GPL).


#include <tamtypes.h>
#include <thbase.h>
#include <thsemap.h>
#include <sifrpc.h>
#include <ioman.h>
#include <sysclib.h>
#include <stddef.h>
#include <stdio.h>
#include <intrman.h>
#include <loadcore.h>
#include <dev9.h>
#include <dmacman.h>

#include "main.h"
#include "smap.h"
#include "smap_eeprom.h"
#include "smap_rpc.h"
#include "dev9_dma.h"
#include "pbuf.h"


SMap SMap0;

u8 smap_rx_buffer[SMAP_RX_BUFFER_SIZE] __attribute((aligned(64)));
static u8 * _data_buffer_pointer = smap_rx_buffer;
#define DATA_BUFFER_USED()	(_data_buffer_pointer-smap_rx_buffer)
#define DATA_BUFFER_FREE()	(SMAP_RX_BUFFER_SIZE-DATA_BUFFER_USED())
#define DATA_BUFFER_RESET()	_data_buffer_pointer = smap_rx_buffer


/*--------------------------------------------------------------------------*/

static void ClearAllIRQs(SMap* pSMap);
static void TXRXEnable(SMap* pSMap);
static void TXRXDisable(SMap* pSMap);
static void TXBDInit(SMap* pSMap);
static void RXBDInit(SMap* pSMap);
static int  FIFOReset(SMap* pSMap);
static int  EMAC3SoftReset(SMap* pSMap);
static void EMAC3SetDefValue(SMap* pSMap);
static void EMAC3Init(SMap* pSMap);
static void Reset(SMap* pSMap);
static void PrintMACAddress(SMap const* pSMap);
static int  GetNodeAddr(SMap* pSMap);
static void BaseInit(SMap* pSMap);
static int  HandleRX(SMap* pSMap);

/*--------------------------------------------------------------------------*/


static u16
ComputeFreeSize(SMapCB const* pCB)
{
	//Compute and return the number of free bytes in pCB.
	u16 u16Start = pCB->u16PTRStart;
	u16 u16End   = pCB->u16PTREnd;

	return (u16Start > u16End) ? (u16Start - u16End) : (pCB->u16Size - (u16End - u16Start));
}


static int
HandleTXInt(int iIntFlags)
{
	SMap* pSMap = &SMap0;
	int iNoError = 1;

	//M_DEBUG("%s\n", __func__);

	while (pSMap->TX.u8IndexStart != pSMap->TX.u8IndexEnd) {
		SMapBD*	pBD = &pSMap->TX.pBD[pSMap->TX.u8IndexStart];
		int iStatus = pBD->ctrl_stat;

		if (iStatus & SMAP_BD_TX_ERRMASK)
			iNoError = 0;

		if (iStatus & SMAP_BD_TX_READY)
			return iNoError;

		// The buffer is sent. Update the start of the active-range (inprogress).
		pSMap->TX.u16PTRStart = (pSMap->TX.u16PTRStart + ((pBD->length + 3) & ~3)) % SMAP_TXBUFSIZE;
		SMAP_BD_NEXT(pSMap->TX.u8IndexStart);

		//Clear BD.
		pBD->length    = 0;
		pBD->pointer   = 0;
		pBD->ctrl_stat = 0;
	}

	if (pSMap->TX.u8IndexStart == pSMap->TX.u8IndexDMA) {
		if (pSMap->TX.u16PTRStart != pSMap->TX.u16PTREnd) {
			M_ERROR("%s: u16PTRStart != u16PTREnd, %d != %d\n", __func__, pSMap->TX.u16PTRStart, pSMap->TX.u16PTREnd);
			pSMap->TX.u16PTRStart = pSMap->TX.u16PTREnd;
		}
		iNoError = 2;
	}

	return iNoError;
}


static void
_rx_smap_done_callback(struct smap_dma_transfer *smaptr, void *arg)
{
	struct pbuf *pBuf = (struct pbuf *)arg;
	SMap* pSMap = &SMap0;

	//M_DEBUG("%s\n", __func__);

	// Clear BD.
	SMAP_REG8(pSMap, SMAP_RXFIFO_FRAME_DEC) = 1;
	pSMap->pRXBD[smaptr->u8Index].ctrl_stat = SMAP_BD_RX_EMPTY;

	// Disable RX interrupts
	//SMap_EnableInterrupts(INTR_RXDNV|INTR_RXEND);

	//if ((u32)smaptr->addr == pBuf->cmd_sg[pBuf->cmd.sg_count-1].addr) {
		// Check for more packets
		//HandleRX(pSMap);

		// Send packets to EE if its the last one
		SMapLowLevelInput(pBuf);
	//}
}

#if 0
#define LOG_SIZE2 (16*1024)
void
log(SMap* pSMap)
{
	static int log_bdcount[SMAP_BD_MAX_ENTRY];
	static int log_count = 0;
	int bdcount;
	u8 rxidx;

	rxidx = pSMap->u8RXIndex;
	for (bdcount = 0; bdcount < SMAP_BD_MAX_ENTRY; bdcount++) {
		if(pSMap->pRXBD[rxidx].ctrl_stat & SMAP_BD_RX_EMPTY)
			break;
		SMAP_BD_NEXT(rxidx);
	}

	log_bdcount[bdcount]++;
	log_count++;

	if (log_count >= LOG_SIZE2) {
		for (log_count = 0; log_count < 12; log_count++)
			M_DEBUG("LOG[%d]: bdcount=%d\n", log_count, log_bdcount[log_count]);
		log_count = 0;

		//for (rxidx = 0; rxidx < SMAP_BD_MAX_ENTRY; rxidx++) {
		//	if (rxidx == pSMap->u8RXIndex)
		//		M_DEBUG("rxidx[%d]=%d <--\n", rxidx, pSMap->pRXBD[rxidx].ctrl_stat);
		//	else
		//		M_DEBUG("rxidx[%d]=%d\n", rxidx, pSMap->pRXBD[rxidx].ctrl_stat);
		//}
	}
}
#endif

static int
HandleRX(SMap* pSMap)
{
	struct pbuf *pBuf = NULL;
	SMapBD *pRXBD;
	int iPKTLen;
	int iPKTLenAligned;
	int iStatus;
	int rv;

	//M_DEBUG("%s\n", __func__);
	//log(pSMap);

	pRXBD = &pSMap->pRXBD[pSMap->u8RXIndex];
	iStatus = pRXBD->ctrl_stat;

	if(iStatus & SMAP_BD_RX_EMPTY)
		return 0;

	iPKTLen = pRXBD->length;
	iPKTLenAligned = (iPKTLen + 3) & ~3;

	if (iStatus & SMAP_BD_RX_ERRMASK) {
		M_DEBUG("%s: packet error\n", __func__);

		//Clear BD.
		SMAP_REG8(pSMap, SMAP_RXFIFO_FRAME_DEC) = 1;
		pRXBD->ctrl_stat = SMAP_BD_RX_EMPTY;

		//Increase the BD-index.
		SMAP_BD_NEXT(pSMap->u8RXIndex);

		return -1;
	}

	if (iPKTLen < SMAP_RXMINSIZE || iPKTLen > SMAP_RXMAXSIZE) {
		M_DEBUG("%s: packet length invalid (%d)\n", __func__, iPKTLen);

		//Clear BD.
		SMAP_REG8(pSMap, SMAP_RXFIFO_FRAME_DEC) = 1;
		pRXBD->ctrl_stat = SMAP_BD_RX_EMPTY;

		//Increase the BD-index.
		SMAP_BD_NEXT(pSMap->u8RXIndex);

		return -2;
	}

	/* Reset the ring-buffer if needed */
	if (DATA_BUFFER_FREE() < iPKTLenAligned)
		DATA_BUFFER_RESET();

	pBuf = pbuf_alloc(iPKTLenAligned, NULL, NULL, _data_buffer_pointer);
	if(pBuf == NULL) {
		M_DEBUG("%s: pBuf == NULL\n", __func__);

		//Clear BD.
		SMAP_REG8(pSMap, SMAP_RXFIFO_FRAME_DEC) = 1;
		pRXBD->ctrl_stat = SMAP_BD_RX_EMPTY;

		//Increase the BD-index.
		SMAP_BD_NEXT(pSMap->u8RXIndex);

		return -1;
	}
	_data_buffer_pointer += iPKTLenAligned;

	// Command to EE
	pBuf->cmd_sg[0].addr = (u32)pBuf->payload;
	pBuf->cmd_sg[0].size = iPKTLen;
	pBuf->cmd.sg_count = 1;

	// Read from SMAP
	pBuf->smaptr[0].addr    = pBuf->payload;
	pBuf->smaptr[0].size    = iPKTLenAligned;
	pBuf->smaptr[0].dir     = DMAC_TO_MEM;
	pBuf->smaptr[0].u16PTR  = ((pRXBD->pointer-SMAP_RXBUFBASE) % SMAP_RXBUFSIZE) & ~3;
	pBuf->smaptr[0].u8Index = pSMap->u8RXIndex;
	pBuf->smaptr[0].cb_done = _rx_smap_done_callback;
	pBuf->smaptr[0].cb_arg  = pBuf;

	//Increase the BD-index.
	SMAP_BD_NEXT(pSMap->u8RXIndex);

	// Transfer the packet
	rv = smap_dma_transfer(&pBuf->smaptr[0]);
	if (rv != 0)
		M_ERROR("%s: smap_dma_transfer returned %d\n", __func__, rv);

	// Disable RX interrupts
	//SMap_DisableInterrupts(INTR_RXDNV|INTR_RXEND);

	return 1;
}


static int
HandleRXInt(int iFlags)
{
	SMap* pSMap = &SMap0;

	//M_DEBUG("%s\n", __func__);

	// Check for more packets
	while(HandleRX(pSMap) != 0)
		;

	return 1;
}


static void
HandleEMAC3IntNr(int nr)
{
	switch (nr) {
	case 0:  /*M_DEBUG("E3_INTR: MMAOP_FAIL\n");*/ break;
	case 1:  /*M_DEBUG("E3_INTR: MMAOP_SUCCESS\n");*/ break;
	case 3:  M_DEBUG("E3_INTR: TX_ERR_1\n"); break;
	case 4:  M_DEBUG("E3_INTR: SQE_ERR_1\n"); break;
	case 5:  M_DEBUG("E3_INTR: DEAD_1\n"); break;
	case 6:  M_DEBUG("E3_INTR: TX_ERR_0\n"); break;
	case 7:  M_DEBUG("E3_INTR: SQE_ERR_0\n"); break;
	case 8:  M_DEBUG("E3_INTR: DEAD_0\n"); break;
	case 9:  M_DEBUG("E3_INTR: DEAD_DEPEND\n"); break;
	case 16: M_DEBUG("E3_INTR: IN_RANGE_ERR\n"); break;
	case 17: M_DEBUG("E3_INTR: OUT_RANGE_ERR\n"); break;
	case 18: M_DEBUG("E3_INTR: TOO_LONG\n"); break;
	case 19: M_DEBUG("E3_INTR: BAD_FCS\n"); break;
	case 20: M_DEBUG("E3_INTR: ALIGN_ERR\n"); break;
	case 21: M_DEBUG("E3_INTR: SHORT_EVENT\n"); break;
	case 22: M_DEBUG("E3_INTR: RUNT_FRAME\n"); break;
	case 23: M_DEBUG("E3_INTR: BAD_FRAME\n"); break;
	case 24: M_DEBUG("E3_INTR: PF\n"); break;
	case 25: M_DEBUG("E3_INTR: OVERRUN\n"); break;
	default: M_DEBUG("E3_INTR: %d\n", nr); break;
	}
}

static void
HandleEMAC3Int(void)
{
	SMap* pSMap = &SMap0;
	u32 u32Status = EMAC3REG_READ(pSMap, SMAP_EMAC3_INTR_STAT);
	int i;

	for (i = 0; i < 32; i++)
		if (u32Status & (1<<i))
			HandleEMAC3IntNr(i);

	//Clear EMAC3 interrupt.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_INTR_STAT, u32Status);
}


/*--------------------------------------------------------------------------*/

static void
ClearAllIRQs(SMap* pSMap)
{
	SMap_ClearIRQ(INTR_ALL);
}


static void
TXRXEnable(SMap* pSMap)
{
	//Enable tx/rx.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_MODE0, E3_TXMAC_ENABLE|E3_RXMAC_ENABLE);
}


static void
TXRXDisable(SMap* pSMap)
{
	int iA;
	u32 u32Val;

	//Disable tx/rx.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_MODE0, EMAC3REG_READ(pSMap, SMAP_EMAC3_MODE0) & (~(E3_TXMAC_ENABLE|E3_RXMAC_ENABLE)));

	//Check EMAC3 idle status.
	for(iA=SMAP_LOOP_COUNT; iA!=0; --iA) {
		u32Val = EMAC3REG_READ(pSMap, SMAP_EMAC3_MODE0);
		if((u32Val & E3_RXMAC_IDLE) && (u32Val & E3_TXMAC_IDLE)) {
			return;
		}
	}
	M_DEBUG("%s: EMAC3 is still running(%x).\n", __func__, (unsigned int)u32Val);
}


static void
TXBDInit(SMap* pSMap)
{
	int iA;

	pSMap->TX.u16PTRStart  = 0;
	pSMap->TX.u16PTREnd    = 0;
	pSMap->TX.u8IndexStart = 0;
	pSMap->TX.u8IndexDMA   = 0;
	pSMap->TX.u8IndexEnd   = 0;
	for(iA = 0;iA<SMAP_BD_MAX_ENTRY;++iA) {
		pSMap->TX.pBD[iA].ctrl_stat = 0;		//Clear ready bit
		pSMap->TX.pBD[iA].reserved  = 0;		//Must be zero
		pSMap->TX.pBD[iA].length    = 0;
		pSMap->TX.pBD[iA].pointer   = 0;
	}
}


static void
RXBDInit(SMap* pSMap)
{
	int iA;

	pSMap->u8RXIndex = 0;
	for(iA=0; iA<SMAP_BD_MAX_ENTRY; ++iA) {
		pSMap->pRXBD[iA].ctrl_stat = SMAP_BD_RX_EMPTY;	//Set empty bit
		pSMap->pRXBD[iA].reserved  = 0;			//Must be zero
		pSMap->pRXBD[iA].length    = 0;
		pSMap->pRXBD[iA].pointer   = 0;
	}
}


static int
FIFOReset(SMap* pSMap)
{
	int iA;
	int iRetVal = 0;

	//Reset TX FIFO.
	SMAP_REG8(pSMap, SMAP_TXFIFO_CTRL) = TXFIFO_RESET;

	//Reset RX FIFO.
	SMAP_REG8(pSMap, SMAP_RXFIFO_CTRL) = RXFIFO_RESET;

	//Confirm reset done.
	for(iA=SMAP_LOOP_COUNT; iA!=0; --iA) {
		if(!(SMAP_REG8(pSMap, SMAP_TXFIFO_CTRL) & TXFIFO_RESET)) {
			break;
		}
	}
	if(iA==0) {
		M_DEBUG("%s: Txfifo reset is in progress\n", __func__);
		iRetVal |= 1;
	}

	for(iA=SMAP_LOOP_COUNT; iA!=0; --iA) {
		if(!(SMAP_REG8(pSMap, SMAP_RXFIFO_CTRL) & RXFIFO_RESET)) {
			break;
		}
	}
	if(iA==0) {
		M_DEBUG("%s: Rxfifo reset is in progress\n", __func__);
		iRetVal |= 2;
	}

	return iRetVal;
}


static int
EMAC3SoftReset(SMap* pSMap)
{
	int iA;

	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_MODE0, E3_SOFT_RESET);
	for(iA=SMAP_LOOP_COUNT; iA!=0; --iA) {
		if(!(EMAC3REG_READ(pSMap, SMAP_EMAC3_MODE0) & E3_SOFT_RESET)) {
			return 0;
		}
	}
	M_DEBUG("%s: EMAC3 reset is in progress\n", __func__);
	return -1;
}


static void
EMAC3SetDefValue(SMap* pSMap)
{
	//Set HW address.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_ADDR_HI, ((pSMap->au8HWAddr[0]<<8)|pSMap->au8HWAddr[1]));
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_ADDR_LO, ((pSMap->au8HWAddr[2]<<24)|(pSMap->au8HWAddr[3]<<16)|(pSMap->au8HWAddr[4]<<8)|pSMap->au8HWAddr[5]));

	//Inter-frame GAP.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_INTER_FRAME_GAP, 4);

	//Rx mode.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_RxMODE, (E3_RX_STRIP_PAD|E3_RX_STRIP_FCS|E3_RX_INDIVID_ADDR|E3_RX_BCAST));

	//Tx fifo value for request priority. low = 7*8=56, urgent = 15*8=120.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_TxMODE1, ((7<<E3_TX_LOW_REQ_BITSFT)|(15<<E3_TX_URG_REQ_BITSFT)));

	//TX threshold, (12+1)*64=832.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_TX_THRESHOLD, ((12&E3_TX_THRESHLD_MSK)<<E3_TX_THRESHLD_BITSFT));

	//Rx watermark, low =  16*8= 128, hi = 128*8=1024.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_RX_WATERMARK, ((16&E3_RX_LO_WATER_MSK)<<E3_RX_LO_WATER_BITSFT)|((128&E3_RX_HI_WATER_MSK)<<E3_RX_HI_WATER_BITSFT));

	// Pause timer
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_PAUSE_TIMER, 0xffff);

	//Enable all EMAC3 interrupts.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_INTR_ENABLE, E3_INTR_ALL);
}


static void
EMAC3Init(SMap* pSMap)
{

	//Reset EMAC3.
	EMAC3SoftReset(pSMap);

	//EMAC3 operating MODE.
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_MODE1, (E3_FDX_ENABLE|E3_IGNORE_SQE|E3_MEDIA_100M|E3_RXFIFO_2K|E3_TXFIFO_1K|E3_TXREQ0_MULTI|E3_TXREQ1_SINGLE));

	//Disable interrupts.
	SMap_DisableInterrupts(INTR_ALL);

	//Clear interruptreq. flags.
	ClearAllIRQs(pSMap);

	//Permanently set to default value.
	EMAC3SetDefValue(pSMap);
}


static void
Reset(SMap* pSMap)
{
	SMap_DisableInterrupts(INTR_ALL);
	ClearAllIRQs(pSMap);

	//BD mode.
	SMAP_REG8(pSMap, SMAP_BD_MODE) = 0;	//Swap

	//Reset TX/RX FIFO.
	FIFOReset(pSMap);

	EMAC3Init(pSMap);
}


static void
PrintMACAddress(SMap const* pSMap)
{
	int iA;

	printf("SMap: Ethernet detected, MAC ");
	for(iA=0; iA<6; ++iA) {
		int iB = pSMap->au8HWAddr[iA];

		printf("%02x",iB);
		if(iA != 5) {
			printf(":");
		}
	}
	printf("\n");
}


static int
GetNodeAddr(SMap* pSMap)
{
	int	iA;
	u16*	pu16MAC = (u16*)pSMap->au8HWAddr;
	u16	u16CHKSum;
	u16	u16Sum = 0;

	ReadFromEEPROM(pSMap, 0x0, pu16MAC, 3);
	ReadFromEEPROM(pSMap, 0x3, &u16CHKSum, 1);

	for(iA=0; iA<3; ++iA) {
		u16Sum+=*pu16MAC++;
	}
	if(u16Sum != u16CHKSum) {
		M_DEBUG("%s: MAC address read error\n", __func__);
		M_DEBUG("checksum %04x is read from EEPROM, and %04x is calculated by mac address read now.\n", u16CHKSum, u16Sum);
		PrintMACAddress(pSMap);
		memset(pSMap->au8HWAddr, 0, 6);
		return -1;
	}
	PrintMACAddress(pSMap);
	return 0;
}


static void
BaseInit(SMap* pSMap)
{

	//We can access register&BD after this routine returned.

	pSMap->pu8Base = (u8 volatile*)SMAP_BASE;
	pSMap->TX.pBD  = (SMapBD*)(pSMap->pu8Base + SMAP_BD_BASE_TX);
	pSMap->pRXBD   = (SMapBD*)(pSMap->pu8Base + SMAP_BD_BASE_RX);

	pSMap->TX.u16Size = SMAP_TXBUFSIZE;
}


u8 const*
SMap_GetMACAddress(void)
{
	SMap* pSMap = &SMap0;

	return pSMap->au8HWAddr;
}


void
SMap_EnableInterrupts(int iFlags)
{
	dev9IntrEnable(iFlags & INTR_ALL);
}


void
SMap_DisableInterrupts(int iFlags)
{
	dev9IntrDisable(iFlags & INTR_ALL);
}


int
SMap_GetIRQ(void)
{
	SMap* pSMap = &SMap0;

	return SMAP_REG16(pSMap, SMAP_INTR_STAT) & INTR_ALL;
}


void
SMap_ClearIRQ(int iFlags)
{
	SMap* pSMap = &SMap0;

	SMAP_REG16(pSMap,SMAP_INTR_CLR) = iFlags & INTR_ALL;
	if(iFlags & INTR_EMAC3) {
		EMAC3REG_WRITE(pSMap, SMAP_EMAC3_INTR_STAT, E3_INTR_ALL);
	}
}


void
SMap_SetLink(SMap* pSMap, u32 iLNK, u32 iFD, u32 i10M)
{
	u32 u32E3Val;

	if (iLNK) {
		M_DEBUG("%s: %s %s duplex mode.\n", __func__, (i10M) ? "10Mbps" : "100Mbps", (iFD) ? "Full" : "Half");

		u32E3Val = EMAC3REG_READ(pSMap, SMAP_EMAC3_MODE1);

		if (iFD) {
			u32E3Val |= (E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);
		}
		else {
			u32E3Val &= ~(E3_FDX_ENABLE|E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);
			if (i10M)
				u32E3Val &= ~E3_IGNORE_SQE;
		}

		// FIXME: Linux driver does not support pause frames
		u32E3Val &= ~(E3_FLOWCTRL_ENABLE|E3_ALLOW_PF);

		u32E3Val &= ~E3_MEDIA_MSK;
		u32E3Val |= i10M ? E3_MEDIA_10M : E3_MEDIA_100M;

		EMAC3REG_WRITE(pSMap, SMAP_EMAC3_MODE1, u32E3Val);
		pSMap->u32Flags |=  SMAP_F_LINKVALID;
	}
	else {
		pSMap->u32Flags &= ~SMAP_F_LINKVALID;
	}
}


int
SMap_Init(void)
{
	SMap* pSMap = &SMap0;

	memset(pSMap, 0, sizeof(*pSMap));

	if (smap_dma_init(pSMap) != 0) {
		return 0;
	}

	BaseInit(pSMap);
	if(GetNodeAddr(pSMap) < 0) {
		return 0;
	}

	Reset(pSMap);
	TXRXDisable(pSMap);
	TXBDInit(pSMap);
	RXBDInit(pSMap);

	return 1;
}


void
SMap_SetMacAddress(const char *addr)
{
	SMap* pSMap = &SMap0;

	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_ADDR_HI,
			addr[0] <<  8 | addr[1]);
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_ADDR_LO,
			addr[2] << 24 | addr[3] << 16 |
			addr[4] <<  8 | addr[5]);
}


void
SMap_Start(void)
{
	SMap* pSMap = &SMap0;

	if (pSMap->u32Flags & SMAP_F_OPENED) {
		return;
	}

	ClearAllIRQs(pSMap);
	TXRXEnable(pSMap);
	SMap_EnableInterrupts(INTR_ALL & ~INTR_TXDNV);

	pSMap->u32Flags |= SMAP_F_OPENED;
}


void
SMap_Stop(void)
{
	SMap* pSMap = &SMap0;

	TXRXDisable(pSMap);
	SMap_DisableInterrupts(INTR_ALL);
	ClearAllIRQs(pSMap);

	pSMap->u32Flags &= ~SMAP_F_OPENED;
}


static void
_tx_smap_done_callback(struct smap_dma_transfer *smaptr, void *arg)
{
	struct pbuf *pBuf = (struct pbuf *)arg;
	SMap* pSMap = &SMap0;
	SMapBD* pTXBD = &pSMap->TX.pBD[smaptr->u8Index];
	SMAP_BD_NEXT(pSMap->TX.u8IndexEnd);

	//M_DEBUG("%s\n", __func__);

	//Send from FIFO to ethernet.
	pTXBD->length = pBuf->len;
	pTXBD->pointer = smaptr->u16PTR + SMAP_TXBUFBASE;
	SMAP_REG8(pSMap, SMAP_TXFIFO_FRAME_INC) = 1;
	pTXBD->ctrl_stat = SMAP_BD_TX_READY|SMAP_BD_TX_GENFCS|SMAP_BD_TX_GENPAD;
	EMAC3REG_WRITE(pSMap, SMAP_EMAC3_TxMODE0, E3_TX_GNP_0);

	SMap_EnableInterrupts(INTR_TXDNV);

	if (pBuf->cb != NULL)
		pBuf->cb(pBuf->cb_arg);
}


SMapStatus
SMap_Send(struct pbuf* pBuf)
{
	SMap* pSMap = &SMap0;
	int iTXLen;
	int rv;

	//Do we have a valid link?
	if (!(pSMap->u32Flags & SMAP_F_LINKVALID)) {
		//No, return SMap_Con to indicate that!
		M_DEBUG("%s: Link not valid\n", __func__);
		return SMap_Con;
	}

	//Is the packetsize in the valid range?
	if (pBuf->len > SMAP_TXMAXSIZE) {
		//No, return SMap_Err to indicate an error occured.
		M_DEBUG("%s: Packet size too large: %d, Max: %d\n", __func__, pBuf->len, SMAP_TXMAXSIZE);
		return SMap_Err;
	}

	//We'll copy whole words to the TX-mem. Compute the number of bytes pBuf will occupy in the TX-mem.
	iTXLen = (pBuf->len + 3) & ~3;

	if (iTXLen > ComputeFreeSize(&pSMap->TX)) {
		return SMap_TX;
	}

	// Write to SMAP
	pBuf->smaptr[0].addr    = pBuf->payload;
	pBuf->smaptr[0].size    = iTXLen;
	pBuf->smaptr[0].dir     = DMAC_FROM_MEM;
	pBuf->smaptr[0].u16PTR  = pSMap->TX.u16PTREnd;
	pBuf->smaptr[0].u8Index = pSMap->TX.u8IndexDMA;
	pBuf->smaptr[0].cb_done = _tx_smap_done_callback;
	pBuf->smaptr[0].cb_arg  = pBuf;

	// Transfer the packet
	rv = smap_dma_transfer(&pBuf->smaptr[0]);
	if (rv != 0)
		M_ERROR("%s: smap_dma_transfer returned %d\n", __func__, rv);

	//Update the end of the active-range.
	pSMap->TX.u16PTREnd = (pSMap->TX.u16PTREnd + iTXLen) % SMAP_TXBUFSIZE;
	SMAP_BD_NEXT(pSMap->TX.u8IndexDMA);

	//Return SMap_OK to indicate success.
	return SMap_OK;
}


int
SMap_HandleTXInterrupt(int iFlags)
{
	if (iFlags & INTR_TXDNV)
		SMap_DisableInterrupts(INTR_TXDNV);
	SMap_ClearIRQ(iFlags);
	return HandleTXInt(iFlags);
}


int
SMap_HandleRXEMACInterrupt(int iFlags)
{
	int iNoError = 1;

	if (iFlags & (INTR_RXDNV|INTR_RXEND)) {
		iFlags &= INTR_RXDNV|INTR_RXEND;
		SMap_ClearIRQ(iFlags);
		if(!HandleRXInt(iFlags)) {
			iNoError = 0;
		}
		iFlags = SMap_GetIRQ();
	}
	if (iFlags & INTR_EMAC3) {
		HandleEMAC3Int();
		SMap_ClearIRQ(INTR_EMAC3);
	}

	return iNoError;
}
