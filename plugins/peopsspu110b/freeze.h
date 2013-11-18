#include "registers.h"
#include "spu.h"
#include "regs.h"


typedef struct
{
	char          szSPUName[8];
	unsigned long ulFreezeVersion;
	unsigned long ulFreezeSize;
	unsigned char cSPUPort[0x200];
	unsigned char cSPURam[0x80000];
	xa_decode_t   xaS;     
} SPUFreeze_t;

typedef struct
{
	unsigned short  spuIrq;
	unsigned long   pSpuIrq;
	unsigned long   bIrqHit;
	unsigned long   dwNoiseCount;
	unsigned long   decoded_ptr;
	unsigned long   dummy3;
	
	SPUCHAN  s_chan[MAXCHAN];   


	// extra data
	unsigned long			spuAddr;
	unsigned short		spuCtrl;
	unsigned short		spuStat;
	unsigned long			dwNoiseVal;
	unsigned long			dwNewChannel;
} SPUOSSFreeze_t;

