/*
 * ppc definitions v0.5.1
 *  Authors: linuzappz <linuzappz@pcsx.net>
 *           alexey silinov
 */

#ifndef __PPC_H__
#define __PPC_H__

// include basic types
#include "../psxcommon.h"
#include "ppc_mnemonics.h"

#define NUM_HW_REGISTERS 25

enum PPCRegs {
	R0 = 0, R1, R2, R3, R4, R5, R6, R7, R8, R9,
	R10, R11, R12, R13, R14, R15, R16, R17, R18, R19,
	R20, R21, R22, R23, R24, R25, R26, R27, R28, R29,
	R30, R31

};

/* general defines */
#define write8(val)  *(u8 *)ppcPtr = val; ppcPtr++;
#define write16(val) *(u16*)ppcPtr = val; ppcPtr+=2;
#define write32(val) *(u32*)ppcPtr = val; ppcPtr+=4;
#define write64(val) *(u64*)ppcPtr = val; ppcPtr+=8;

#define CALLFunc(FUNC) \
{ \
    u32 _func = (FUNC); \
    if ((_func & 0x1fffffc) == _func) { \
        BLA(_func); \
    } else { \
        LIW(0, _func); \
        MTCTR(0); \
        BCTRL(); \
    } \
}

extern int cpuHWRegisters[NUM_HW_REGISTERS];

extern u32 *ppcPtr;
extern u8  *j8Ptr[32];
extern u32 *j32Ptr[32];

void ppcInit();
void ppcSetPtr(u32 *ptr);
void ppcShutdown();

void ppcAlign(int bytes);
void returnPC();
void recRun(void (*func)(), u32 hw1, u32 hw2);
u8 dynMemRead8(u32 mem);
u16 dynMemRead16(u32 mem);
u32 dynMemRead32(u32 mem);
void dynMemWrite32(u32 mem, u32 val);

#endif /* __PPC_H__ */





















