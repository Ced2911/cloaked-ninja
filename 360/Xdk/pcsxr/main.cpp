#include <xtl.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "psxcommon.h"
#include "cdriso.h"
#include "r3000a.h"

char * game = "game:\\Castlevanina - SOTN.bin";
//char * game = "game:\\psxisos\\castle.bin";

//--------------------------------------------------------------------------------------
// Name: ClearCaches
// Desc: This function gets the caches into a known state, with no useful data in them.
//       It doesn't clear the instruction caches, but the data caches should be
//       thoroughly flushed.
//--------------------------------------------------------------------------------------
// Turn off all optimizations so that the useless code below won't be optimized away.
#pragma optimize("", off)
extern "C" void ClearCaches()
{
	const size_t MemSize = 2000000;
	char* pMemory = new char[MemSize];

	// Zero the newly allocated memory - this gets as much of it as will
	// fit into the L2 cache. This step isn't strictly necessary, but it avoids
	// any potential warnings about reading from uninitialized memory.
	XMemSet( pMemory, 0, MemSize );

	// Now loop through, reading from every byte. This pulls the data into the
	// L1 cache - as much of it as will fit.
	DWORD gSum = 0;
	for( int i = 0; i < MemSize; ++i )
	{
		gSum += pMemory[i];
	}

	// Now flush the data out of the caches. This should leave the L1 and L2
	// caches virtually empty.
	for( int i = 0; i < MemSize; i += 128 )
		__dcbf( i, pMemory );

	delete [] pMemory;
}
// Restore optimizations.
#pragma optimize("", on)


extern "C" int funct (register void (*func)(), register u32 hw1, register u32 hw2);
int main(){
	
	// __SetHWThreadPriorityHigh();
	SetIsoFile(game);

	int res, ret;
	XMemSet(&Config, 0, sizeof(PcsxConfig));
	
	Config.Cpu = CPU_INTERPRETER;
	// Config.Cpu = CPU_DYNAREC;

	strcpy(Config.Bios, "SCPH1001.BIN"); // Use actual BIOS
	//strcpy(Config.Bios, "HLE"); // Use HLE
	strcpy(Config.BiosDir, "game:\\BIOS");
	strcpy(Config.Mcd1,"game:\\BIOS\\Memcard1.mcd");
	strcpy(Config.Mcd2,"game:\\BIOS\\Memcard2.mcd");

	Config.PsxOut = 0;
	Config.HLE = 1;
	Config.Xa = 0;  //XA enabled
	Config.Cdda = 0;
	Config.PsxAuto = 0; //Autodetect
	
	cdrIsoInit();

	if (SysInit() == -1) 
	{
		printf("SysInit() Error!\n");
		return E_FAIL;
	}

	GPU_clearDynarec(clearDynarec);

	ret = CDR_open();
	if (ret < 0) { SysMessage (_("Error Opening CDR Plugin")); return -1; }
	ret = GPU_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening GPU Plugin (%d)"), ret); return -1; }
	ret = SPU_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening SPU Plugin (%d)"), ret); return -1; }
	ret = PAD1_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening PAD1 Plugin (%d)"), ret); return -1; }

	CDR_init();
	GPU_init();
	SPU_init();
	PAD1_init(1);
	PAD2_init(2);
	
	SysReset();

	SysPrintf("CheckCdrom\r\n");
	res = CheckCdrom();
	if(res)
		SysPrintf("CheckCdrom: %08x\r\n",res);
	res=LoadCdrom();
	if(res)
		SysPrintf("LoadCdrom: %08x\r\n",res);

	SysPrintf("Execute\r\n");

	psxCpu->Execute();
}