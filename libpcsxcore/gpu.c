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

// Select the good one !
//#define THREAD_GPU_DMA
#define THREAD_GPU_WRITE

static volatile	uint32_t gpu_thread_running = 0;
static volatile uint32_t gpu_thread_exit = 0;
static volatile uint32_t dma_addr;


#ifdef THREAD_GPU_DMA

static __inline void WaitForGpuThread() {
    while(gpu_thread_running) {
		YieldProcessor(); // or r31, r31, r31
	}

	// High priority
	__asm{
		or r3, r3, r3
	};
}

static void gpuThread() {
	while(!gpu_thread_exit) {
		if (gpu_thread_running) {
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

				if(count>0) {
					// Call real gpu ptr func
					GPU_writeDataMem(&baseAddrL[dmaMem>>2],count);
				}

				addr = __loadwordbytereverse(0, &baseAddrL[addr>>2])&0xffffff;
				//addr = psxMu32( addr >> 2 ) & 0xffffff;
			}
			while (addr != 0xffffff);

			gpu_thread_running = 0;
		}
	}
}

void gpuDmaChain(uint32_t addr)
{
	WaitForGpuThread();
	
	dma_addr = addr;

	gpu_thread_running = 1;
}

void gpuWriteDataMem(uint32_t * addr, int size) {
	GPU_writeDataMem(addr ,size);
}

#endif

#ifdef THREAD_GPU_WRITE

#define TW_RING_MAX_COUNT (128*1024)

#define tw_ring_count(str) ((u32)labs((str[1]%TW_RING_MAX_COUNT)-(str[0]%TW_RING_MAX_COUNT)))
#define tw_read_idx tw_idx[0]
#define tw_write_idx tw_idx[1]

static __declspec(align(128)) uint32_t tw_ring[TW_RING_MAX_COUNT];

static volatile  __declspec(align(128))  uint64_t tw_idx[2] = {0,0};

static __inline void WaitForGpuThread() {
    while(gpu_thread_running||tw_ring_count(tw_idx)>0) {
		YieldProcessor(); // or r31, r31, r31
	}

	// High priority
	__asm{
		or r3, r3, r3
	};
}

static void gpuThread() {
	uint64_t  __declspec(align(128)) lidx[2];
	__vector4 vt;

	while(!gpu_thread_exit) {

		// atomic ...
		vt = __lvx((void*)tw_idx, 0);
		__stvx(vt, lidx, 0);

		if(tw_ring_count(lidx)!=0)
        {
            uint32_t ri=lidx[0]%TW_RING_MAX_COUNT;
            uint32_t rc=tw_ring_count(lidx);
            
            uint32_t chunk=min(rc,(TW_RING_MAX_COUNT-ri));
            uint32_t * chunk_start=&tw_ring[ri];

            gpu_thread_running = 1;

			GPU_writeDataMem(chunk_start, chunk);
            
            tw_read_idx+=chunk;
            
            gpu_thread_running = 0;
        }
	}
}

// Queue write
void gpuWriteDataMem(uint32_t * pMem, int size) {
	u32 * lda=pMem;
    u32 wi=tw_write_idx;

    while(size>TW_RING_MAX_COUNT-tw_ring_count(tw_idx)) 
		YieldProcessor(); // or r31, r31, r31
    
    while((lda-pMem)<size)
    {
		// Copy data ...
        u32 * d =&tw_ring[wi%TW_RING_MAX_COUNT];

		*d=*lda;

        ++wi;
        ++lda;
    }

    tw_write_idx+=size;
}

// Classic call !
void gpuDmaChain(uint32_t addr)
{
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

		if(count>0) {
			// Call threaded gpu func
			gpuWriteDataMem(&baseAddrL[dmaMem>>2],count);
		}

		addr = __loadwordbytereverse(0, &baseAddrL[addr>>2])&0xffffff;
		//addr = psxMu32( addr >> 2 ) & 0xffffff;
	}
	while (addr != 0xffffff);
}

#endif

void gpuWriteData(uint32_t v) {
#ifdef THREAD_GPU_DMA
	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 
#endif
	v = __loadwordbytereverse(0, &v);
	gpuWriteDataMem(&v, 1);
}

void gpuUpdateLace() {
	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 
	GPU_updateLace();
}

void gpuWriteStatus(u32 data) {
#ifdef THREAD_GPU_WRITE
	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 
#endif
	GPU_writeStatus(data);
}

void gpuReadDataMem(uint32_t * addr, int size) {
#ifdef THREAD_GPU_WRITE
	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 
#endif
	GPU_readDataMem(addr, size);
}

void gpuDmaThreadInit() {
	HANDLE gpuHandle = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)gpuThread, NULL, CREATE_SUSPENDED, NULL);

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
			gpuReadDataMem(ptr, size);
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
			//gpuWriteDataMemThread(ptr, size);

			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(size);
			return;

		case 0x01000401: // dma chain
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU dma chain *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif

			size = gpuDmaChainSize(madr);

			gpuDmaChain(madr & VM_MASK);
			
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
