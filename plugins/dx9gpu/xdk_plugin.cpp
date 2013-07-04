#include <xtl.h>
#include "stdafx.h"
#include "externals.h"
extern "C" {
	#include "psxcommon.h"
	#include "plugins.h"
}
#if 0

typedef struct GPUFREEZETAG
{
	uint32_t ulFreezeVersion;      // should be always 1 for now (set by main emu)
	uint32_t ulStatus;             // current gpu status
	uint32_t ulControl[256];       // latest control register values
	unsigned char psxVRam[1024*1024*2]; // current VRam image (full 2 MB for ZN)
} HW_GPUFreeze_t;

// Gpu entry points
void CALLBACK HW_GPUmakeSnapshot(void);
long CALLBACK HW_GPUopen(HWND hwndGPU);
long CALLBACK HW_GPUclose();
long CALLBACK HW_GPUshutdown();
long CALLBACK HW_GPUconfigure(void);
void CALLBACK HW_GPUabout(void);
long CALLBACK HW_GPUtest(void);
long CALLBACK HW_GPUinit();
void CALLBACK HW_GPUcursor(int iPlayer,int x,int y);
void CALLBACK HW_GPUupdateLace(void);
uint32_t CALLBACK HW_GPUreadStatus(void);
void CALLBACK HW_GPUwriteStatus(uint32_t gdata);
void CALLBACK HW_GPUreadDataMem(uint32_t *pMem, int iSize);
uint32_t CALLBACK HW_GPUreadData(void);
void CALLBACK HW_GPUwriteDataMem(uint32_t *pMem, int iSize);
void CALLBACK HW_GPUwriteData(uint32_t gdata);
long CALLBACK HW_GPUdmaChain(uint32_t *baseAddrL, uint32_t addr);
long CALLBACK HW_GPUfreeze(uint32_t ulGetFreezeData,HW_GPUFreeze_t * pF);
void CALLBACK HW_GPUvBlank( int val );

extern "C" void GpuHwInit() {
	GPU_open = HW_GPUopen;
	GPU_init = HW_GPUinit;
	GPU_close = HW_GPUclose;
	GPU_updateLace = HW_GPUupdateLace;
	GPU_shutdown = HW_GPUshutdown;
	gpu_re
}
#endif