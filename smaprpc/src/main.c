/*
 * Copyright (c) Tord Lindstrom (pukko@home.se)
 * Copyright (c) adresd ( adresd_ps2dev@yahoo.com )
 * Copyright 2009 Mega Man
 *
 * Based on ps2smap. Code changed for kernelloader.
 *
 * Branched from:
 *    svn://svn.ps2dev.org/ps2/trunk/ps2eth
 *    Revision: 1588
 *
 * Purpose: Forward ethernet frame between EE and IOP,
 *          because slim PSTwo doesn't have direct access
 *          to the IOP hardware.
 */

#include <stdio.h>
#include <sysclib.h>
#include <loadcore.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <vblank.h>
#include <intrman.h>
#include <sifman.h>
#include <sifcmd.h>
#include <sifrpc.h>

#include "smap.h"
#include "dev9.h"
#include "smap_cmd.h"
#include "smap_rpc.h"
#include "dev9_dma.h"
#include "pbuf.h"
#include "main.h"

IRX_ID(MODNAME, 1, 0);

#define	UNKN_1464   *(u16 volatile*)0xbf801464

#define	TIMER_INTERVAL		(100*1000)
#define	TIMEOUT			(300*1000)
#define	MAX_REQ_CNT		64


static int 		iReqNR=0;
static int 		iReqCNT=0;
static PBuf*		apReqQueue[MAX_REQ_CNT];
static int		iTimeoutCNT=0;

u32 ee_buffer;
u32 ee_buffer_pos;
u32 volatile ee_buffer_size = 0;


static void
StoreLast(PBuf* pBuf)
{

	//Store pBuf last in the request-queue.

	apReqQueue[(iReqNR+iReqCNT)%MAX_REQ_CNT] = pBuf;
	iReqCNT++;

	//Since pBuf won't be sent right away, increase the reference-count to prevent it from being deleted before it's sent.
	pBuf->ref++;
}


static PBuf*
GetFirst(void)
{
	return apReqQueue[iReqNR];
}


static void
PopFirst(void)
{
	iReqNR = (iReqNR + 1) % MAX_REQ_CNT;
	iReqCNT--;
}


static void
tx_queue_reset(void)
{
	M_DEBUG("%s: iReqCNT = %d\n", __func__, iReqCNT);

	iReqNR = 0;
	iReqCNT = 0;
	iTimeoutCNT = 0;
}


void
smap_reset(void)
{
	int iIntFlags;

	CpuSuspendIntr(&iIntFlags);

	tx_queue_reset();
	pbuf_reset();

	CpuResumeIntr(iIntFlags);
}


SMapStatus
SMapLowLevelOutput(PBuf* pBuf)
{
	int iIntFlags;
	SMapStatus Ret;

	CpuSuspendIntr(&iIntFlags);

	if (iReqCNT == 0) {
		Ret = SMap_Send(pBuf);

		// Reply
		if (Ret != SMap_OK) {
			if (Ret == SMap_TX) {
				// TX buffer full, place in queue so we can send it later
				StoreLast(pBuf);
				iTimeoutCNT = 0;
				Ret = SMap_OK;
			}
			else {
				M_ERROR("%s\n", __func__);
				// ERROR: callback user
				pBuf->cmd._spare = 0xffffffff;
				if (pBuf->cb != NULL)
					pBuf->cb(pBuf->cb_arg);
			}
		}
	}
	else if	(iReqCNT < MAX_REQ_CNT) {
		StoreLast(pBuf);
		Ret = SMap_OK;
	}
	else {
		Ret = SMap_TX;
	}

	CpuResumeIntr(iIntFlags);

	return Ret;
}


static void
SendRequests(void)
{
	while (iReqCNT > 0) {
		PBuf *pBuf = GetFirst();
		SMapStatus Status = SMap_Send(pBuf);

		if (Status == SMap_TX) {
			iTimeoutCNT = 0;
			return;
		}

		if (Status != SMap_OK) {
			M_ERROR("%s\n", __func__);
			// ERROR: callback user
			pBuf->cmd._spare = 0xffffffff;
			if (pBuf->cb != NULL)
				pBuf->cb(pBuf->cb_arg);
		}

		PopFirst();
	}
}


static int
SMapInterrupt(int iFlag)
{
	int iFlags = SMap_GetIRQ();

	if (iFlags & (INTR_TXDNV|INTR_TXEND)) {
		//It's a TX-interrupt, handle it now!
		SMap_HandleTXInterrupt(iFlags & (INTR_TXDNV|INTR_TXEND));

		//Handle the request-queue.
		SendRequests();

		//Several packets might have been sent during SendRequests and we might have spent a couple of 1000 usec. Re-read the
		//interrupt-flags to more accurately reflect the current interrupt-status.
		iFlags = SMap_GetIRQ();
	}

	if (iFlags & (INTR_EMAC3|INTR_RXDNV|INTR_RXEND)) {
		//It's a RX- or a EMAC-interrupt, handle it!
		SMap_HandleRXEMACInterrupt(iFlags & (INTR_EMAC3|INTR_RXDNV|INTR_RXEND));
	}

	return 1;
}


static unsigned int
Timer(void* pvArg)
{
	if (iReqCNT <= 0) {
		iTimeoutCNT = 0;
	}
	else {
		iTimeoutCNT += TIMER_INTERVAL;
		if (iTimeoutCNT >= TIMEOUT) {
			M_ERROR("%s: TX timeout!\n", __func__);
			SendRequests();
			iTimeoutCNT = 0;
		}
	}

	return (unsigned int)pvArg;
}


static void
InstallIRQHandler(void)
{
	int iA;

	for (iA=2;iA<7;++iA) {
		dev9RegisterIntrCb(iA, SMapInterrupt);
	}
}


static void
InstallTimer(void)
{
	iop_sys_clock_t	ClockTicks;

	USec2SysClock(TIMER_INTERVAL, &ClockTicks);
	SetAlarm(&ClockTicks, Timer, (void*)ClockTicks.lo);
}


static void
DetectAndInitDev9(void)
{
	SMap_DisableInterrupts(INTR_ALL);
	EnableIntr(IOP_IRQ_DEV9);
	CpuEnableIntr();

	UNKN_1464=3;
}


void
SMapLowLevelInput(PBuf* pBuf)
{
	struct ps2_smap_cmd_rw *cmd = &pBuf->cmd;
	struct ps2_smap_sg *cmd_sg = &pBuf->cmd_sg[0];
	int i;

	if (ee_buffer_size <= 0) {
		M_ERROR("%s: Loosing ethernet frame. No EE rx buffer\n", __func__);
		return;
	}

	if ((ee_buffer_pos + pBuf->len) > ee_buffer_size) {
		ee_buffer_pos = 0;
	}

	/* Change from IOP addr to EE addr */
	for (i = 0; i < cmd->sg_count; i++)
		cmd_sg[i].addr = (ee_buffer + ee_buffer_pos) + (cmd_sg[i].addr - cmd_sg[0].addr);

	pBuf->id = isceSifSendCmd(SIF_SMAP_RECEIVE, cmd, sizeof(struct ps2_smap_cmd_rw) + cmd->sg_count * sizeof(struct ps2_smap_sg), pBuf->payload, (void *)cmd_sg[0].addr, pBuf->len);
	if (pBuf->id == 0) {
		M_ERROR("%s: isceSifSendCmd failed\n", __func__);
		pbuf_free(pBuf);
		return;
	}

	ee_buffer_pos = (ee_buffer_pos + pBuf->len + DMA_TRANSFER_SIZE - 1) & ~(DMA_TRANSFER_SIZE - 1);
}


int
_start(int argc, char *argv[])
{
	if (!sceSifCheckInit())
		sceSifInit();
	sceSifInitRpc(0);

	DetectAndInitDev9();

	if (!SMap_Init()) {
		M_ERROR("failed to init smap\n");
		return	MODULE_NO_RESIDENT_END;
	}

	InstallIRQHandler();

	InstallTimer();

	SMap_Start();

	if(smap_cmd_init() != 0) {
		M_ERROR("failed to init cmd\n");
		return MODULE_NO_RESIDENT_END;
	}

	if(smap_rpc_init() != 0) {
		M_ERROR("failed to init rpc\n");
		return MODULE_NO_RESIDENT_END;
	}

	M_PRINTF("running\n");

	return	MODULE_RESIDENT_END;
}
