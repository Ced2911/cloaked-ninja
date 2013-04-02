/*  Copyright (c) 2010, shalma.
 *  Portions Copyright (c) 2002, Pete Bernert.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include "psxhw.h"
#include "gpu.h"
#include "psxdma.h"

extern unsigned int hSyncCount;

#define GPUSTATUS_ODDLINES            0x80000000
#define GPUSTATUS_DMABITS             0x60000000 // Two bits
#define GPUSTATUS_READYFORCOMMANDS    0x10000000
#define GPUSTATUS_READYFORVRAM        0x08000000
#define GPUSTATUS_IDLE                0x04000000

#define GPUSTATUS_DISPLAYDISABLED     0x00800000
#define GPUSTATUS_INTERLACED          0x00400000
#define GPUSTATUS_RGB24               0x00200000
#define GPUSTATUS_PAL                 0x00100000
#define GPUSTATUS_DOUBLEHEIGHT        0x00080000
#define GPUSTATUS_WIDTHBITS           0x00070000 // Three bits
#define GPUSTATUS_MASKENABLED         0x00001000
#define GPUSTATUS_MASKDRAWN           0x00000800
#define GPUSTATUS_DRAWINGALLOWED      0x00000400
#define GPUSTATUS_DITHER              0x00000200

// Taken from PEOPS SOFTGPU
u32 lUsedAddr[3];

static __inline boolean CheckForEndlessLoop(u32 laddr) {
	if (laddr == lUsedAddr[1]) return TRUE;
	if (laddr == lUsedAddr[2]) return TRUE;

	if (laddr < lUsedAddr[0]) lUsedAddr[1] = laddr;
	else lUsedAddr[2] = laddr;

	lUsedAddr[0] = laddr;

	return FALSE;
}

static u32 gpuDmaChainSize(u32 addr) {
	u32 size;
	u32 DMACommandCounter = 0;
	lUsedAddr[0] = lUsedAddr[1] = lUsedAddr[2] = 0xffffff;

	// initial linked list ptr (word)
	size = 1;

	do {
		addr &= 0x1ffffc;

		if (DMACommandCounter++ > 2000000) break;
		if (CheckForEndlessLoop(addr)) break;


		// # 32-bit blocks to transfer
		size += psxMu8( addr + 3 );

		
		// next 32-bit pointer
		addr = psxMu32( addr & ~0x3 ) & 0xffffff;
		//addr = __loadwordbytereverse(0,&baseAddrL[addr>>2])&0xffffff;
		size += 1;
	} while (addr != 0xffffff);

	
	return size;
}

int gpuReadStatus() {
	int hard;
	
	// GPU plugin
	hard = GPU_readStatus();

	// Gameshark Lite - wants to see VRAM busy
	// - Must enable GPU 'Fake Busy States' hack
	if( (hard & GPUSTATUS_IDLE) == 0 )
		hard &= ~GPUSTATUS_READYFORVRAM;

	return hard;
}

void gpuDmaChain(uint32_t addr)
{
	uint32_t dmaMem;
	unsigned char * baseAddrB;
	short count;
	unsigned int DMACommandCounter = 0;
	uint32_t * baseAddrL = (u32 *)psxM;


	lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

	baseAddrB = (unsigned char*) baseAddrL;

	// Must be in a thread ?!
	do
	{
		addr&=0x1FFFFC;
		if(DMACommandCounter++ > 2000000) break;
		if(CheckForEndlessLoop(addr)) break;

		count = baseAddrB[addr+3];

		dmaMem=addr+4;

		if(count>0) 
			GPU_writeDataMem(&baseAddrL[dmaMem>>2],count);

		addr = __loadwordbytereverse(0, &baseAddrL[addr>>2])&0xffffff;
		//addr = psxMu32( addr >> 2 ) & 0xffffff;
	}
	while (addr != 0xffffff);
}

static volatile	uint32_t dma_thread_running = 0;
static volatile uint32_t dma_addr;
static volatile uint32_t dma_thread_exit = 0;

static void WaitForGpuThread() {
    while(dma_thread_running) {
		YieldProcessor(); // or r31, r31, r31
	}

	// High priority
	__asm{
		or r3, r3, r3
	};
}

void gpuWriteData(uint32_t v) {
	if(!dma_thread_exit) {
		WaitForGpuThread();
	} 
	GPU_writeData(v);
}

void gpuUpdateLace() {
	if(!dma_thread_exit) {
		WaitForGpuThread();
	} 
	GPU_updateLace();
}

static void gpuDmaThread() {
	while(!dma_thread_exit) {
		if (dma_thread_running) {
			uint32_t addr = dma_addr;
			uint32_t dmaMem;
			unsigned char * baseAddrB;
			short count;
			unsigned int DMACommandCounter = 0;
			uint32_t * baseAddrL = (u32 *)psxM;


			lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

			baseAddrB = (unsigned char*) baseAddrL;

			do
			{
				addr&=0x1FFFFC;
				if(DMACommandCounter++ > 2000000) break;
				if(CheckForEndlessLoop(addr)) break;

				count = baseAddrB[addr+3];

				dmaMem=addr+4;

				if(count>0) 
					GPU_writeDataMem(&baseAddrL[dmaMem>>2],count);

				addr = __loadwordbytereverse(0, &baseAddrL[addr>>2])&0xffffff;
				//addr = psxMu32( addr >> 2 ) & 0xffffff;
			}
			while (addr != 0xffffff);

			dma_thread_running = 0;
		}
	}
}

void gpuDmaChainThread(uint32_t addr)
{
	WaitForGpuThread();
	
	dma_addr = addr;

	dma_thread_running = 1;
}

void gpuDmaThreadInit() {
	HANDLE gpuHandle = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)gpuDmaThread, NULL, CREATE_SUSPENDED, NULL);

	XSetThreadProcessor(gpuHandle, 2);
	//SetThreadPriority(threadid ,THREAD_PRIORITY_HIGHEST);
	ResumeThread(gpuHandle);
}

void psxDma2(u32 madr, u32 bcr, u32 chcr) { // GPU
	u32 *ptr;
	u32 size;

	switch (chcr) {
		case 0x01000200: // vram2mem
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA2 GPU - vram2mem *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif
			ptr = (u32 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - vram2mem *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			size = (bcr >> 16) * (bcr & 0xffff);
			GPU_readDataMem(ptr, size);
			psxCpu->Clear(madr, size);

			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(size);
			return;

		case 0x01000201: // mem2vram
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU mem2vram *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif
			ptr = (u32 *)PSXM(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - mem2vram *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			size = (bcr >> 16) * (bcr & 0xffff);
			GPU_writeDataMem(ptr, size);

			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(size);
			return;

		case 0x01000401: // dma chain
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU dma chain *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif

			size = gpuDmaChainSize(madr);

			//GPU_dmaChain((u32 *)psxM, madr & 0x1fffff);

			//__GPUdmaChain(madr & 0x1fffff);
			//gpuDmaChain(madr & 0x1fffff);
			gpuDmaChainThread(madr & 0x1fffff);
			
			// Tekken 3 = use 1.0 only (not 1.5x)

			// Einhander = parse linked list in pieces (todo)
			// Final Fantasy 4 = internal vram time (todo)
			// Rebel Assault 2 = parse linked list in pieces (todo)
			// Vampire Hunter D = allow edits to linked list (todo)
			GPUDMA_INT(size);
			return;

#ifdef PSXDMA_LOG
		default:
			PSXDMA_LOG("*** DMA 2 - GPU unknown *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
			break;
#endif
	}

	HW_DMA2_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(2);
}

void gpuInterrupt() {
	HW_DMA2_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(2);
}
