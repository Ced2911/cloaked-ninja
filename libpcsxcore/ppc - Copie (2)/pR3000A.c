/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2003  Pcsx Team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
//#include <inttypes.h> 
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "psxcommon.h"
#include "ppc.h"
#include "reguse.h"
#include "pR3000A.h"
#include "r3000a.h"
#include "psxhle.h"
#include "cdrom.h"
#include "mdec.h"

#include "libxenon_vm.h"

// fast hack !!
#define malloc	balloc
#define free	bfree

int do_disasm = 0;
static int force_disasm = 0;

void __declspec(naked) __icbi(int offset, const void * base)
{
	__asm {
        icbi r3,r4
        blr
    }
}

static void __inline invalidateCache(u32 from, u32 to) {
    while(from < to) {
            __icbi(0, (void*)from);
			__dcbst(0, (void*)from);
            from += 4;
    }
    __emit(0x7c0004ac);//sync
	__emit(0x4C00012C);//isync
}

/* variable declarations */
__attribute__((aligned(65536))) u32 psxRecLUT[0x010000];

__attribute__((aligned(65536),section(".bss.beginning.upper"))) char recMem[RECMEM_SIZE];
static char recRAM[0x200000];
static char recROM[0x080000];

static u32 pc; /* recompiler pc */
static uint32_t pcold; /* recompiler oldpc */
static int count; /* recompiler intruction count */
static int branch; /* set for branch */
static u32 target; /* branch target */
iRegisters iRegs[34];

int psxCP2time[64] = {
    2, 16, 1, 1, 1, 1, 8, 1, // 00
    1, 1, 1, 1, 6, 1, 1, 1, // 08
    8, 8, 8, 19, 13, 1, 44, 1, // 10
    1, 1, 1, 17, 11, 1, 14, 1, // 18
    30, 1, 1, 1, 1, 1, 1, 1, // 20
    5, 8, 17, 1, 1, 5, 6, 1, // 28
    23, 1, 1, 1, 1, 1, 1, 1, // 30
    1, 1, 1, 1, 1, 6, 5, 39 // 38
};

static void (*recBSC[64])();
static void (*recSPC[64])();
static void (*recREG[32])();
static void (*recCP0[32])();
static void (*recCP2[64])();
static void (*recCP2BSC[32])();

static HWRegister HWRegisters[NUM_HW_REGISTERS];
static int HWRegUseCount;
static int UniqueRegAlloc;

void recRecompile();
static void recError();

// used in debug.c for dynarec free space printing
u32 dyna_used = 0;
u32 dyna_total = RECMEM_SIZE;

/* --- Generic register mapping --- */

int GetFreeHWReg() {
    int i, least, index;

	// LRU algorith with a twist ;)
	for (i = 0; i < NUM_HW_REGISTERS; i++) {
		if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
			break;
		}
	}

	least = HWRegisters[i].lastUsed;
	index = i;
	for (; i < NUM_HW_REGISTERS; i++) {
		if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
			if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
			    if (HWRegisters[i].usage == HWUSAGE_NONE && HWRegisters[i].code >= 13) {
				    index = i;
				    break;
			    }
			    else if (HWRegisters[i].lastUsed < least) {
				    least = HWRegisters[i].lastUsed;
				    index = i;
			    }
		    }
		}
	}

	// Cycle the registers
	if (HWRegisters[index].usage == HWUSAGE_NONE) {
		for (; i < NUM_HW_REGISTERS; i++) {
			if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
				if (HWRegisters[i].usage == HWUSAGE_NONE &&
					HWRegisters[i].code >= 13 && 
					HWRegisters[i].lastUsed < least) {
					least = HWRegisters[i].lastUsed;
					index = i;
					break;
				}
			}
		}
	}

    /*	if (HWRegisters[index].code < 13 && HWRegisters[index].code > 3) {
                    SysPrintf("Allocating volatile register %i\n", HWRegisters[index].code);
            }
            if (HWRegisters[index].usage != HWUSAGE_NONE) {
                    SysPrintf("RegUse too big. Flushing %i\n", HWRegisters[index].code);
            }*/
    if (HWRegisters[index].usage & (HWUSAGE_RESERVED | HWUSAGE_HARDWIRED)) {
        if (HWRegisters[index].usage & HWUSAGE_RESERVED) {
            SysPrintf("Error! Trying to map a new register to a reserved register (r%i)",
                    HWRegisters[index].code);
        }
        if (HWRegisters[index].usage & HWUSAGE_HARDWIRED) {
            SysPrintf("Error! Trying to map a new register to a hardwired register (r%i)",
                    HWRegisters[index].code);
        }
    }

    if (HWRegisters[index].lastUsed != 0) {
        UniqueRegAlloc = 0;
    }

    // Make sure the register is really flushed!
    FlushHWReg(index);
    HWRegisters[index].usage = HWUSAGE_NONE;
    HWRegisters[index].flush = NULL;

    return index;
}

void FlushHWReg(int index) {
    if (index < 0) return;
    if (HWRegisters[index].usage == HWUSAGE_NONE) return;

    if (HWRegisters[index].flush) {
        HWRegisters[index].usage |= HWUSAGE_RESERVED;
        HWRegisters[index].flush(index);
        HWRegisters[index].flush = NULL;
    }

    if (HWRegisters[index].usage & HWUSAGE_HARDWIRED) {
        HWRegisters[index].usage &= ~(HWUSAGE_READ | HWUSAGE_WRITE);
    } else {
        HWRegisters[index].usage = HWUSAGE_NONE;
    }
}

// get rid of a mapped register without flushing the contents to the memory

void DisposeHWReg(int index) {
    if (index < 0) return;
    if (HWRegisters[index].usage == HWUSAGE_NONE) return;

    HWRegisters[index].usage &= ~(HWUSAGE_READ | HWUSAGE_WRITE);
    if (HWRegisters[index].usage == HWUSAGE_NONE) {
        SysPrintf("Error! not correctly disposing register (r%i)", HWRegisters[index].code);
    }

    FlushHWReg(index);
}

// operated on cpu registers

__inline static void FlushCPURegRange(int start, int end) {
    int i;

    if (end <= 0) end = 31;
    if (start <= 0) start = 0;

    for (i = 0; i < NUM_HW_REGISTERS; i++) {
        if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
            if (HWRegisters[i].flush)
                FlushHWReg(i);
    }

    for (i = 0; i < NUM_HW_REGISTERS; i++) {
        if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
            FlushHWReg(i);
    }
}

void FlushAllHWReg() {
    FlushCPURegRange(0, 31);
}

void InvalidateCPURegs() {
	// nothing
}

/* --- Mapping utility functions --- */

int UpdateHWRegUsage(int hwreg, int usage) {
    HWRegisters[hwreg].lastUsed = ++HWRegUseCount;
    if (usage & HWUSAGE_WRITE) {
        HWRegisters[hwreg].usage &= ~HWUSAGE_CONST;
    }
    if (!(usage & HWUSAGE_INITED)) {
        HWRegisters[hwreg].usage &= ~HWUSAGE_INITED;
    }
    HWRegisters[hwreg].usage |= usage;

    return HWRegisters[hwreg].code;
}

int GetHWRegFromCPUReg(int cpureg) {
    int i;
    for (i = 0; i < NUM_HW_REGISTERS; i++) {
        if (HWRegisters[i].code == cpureg) {
            return i;
        }
    }

    SysPrintf("Error! Register location failure (r%i)", cpureg);
    return 0;
}

/* --- Psx register mapping --- */

void MapPsxReg32(int reg) {
    int hwreg = GetFreeHWReg();
    HWRegisters[hwreg].flush = FlushPsxReg32;
    HWRegisters[hwreg].private = reg;

    if (iRegs[reg].reg != -1) {
        SysPrintf("error: double mapped psx register");
    }

    iRegs[reg].reg = hwreg;
    iRegs[reg].state |= ST_MAPPED;
}

void FlushPsxReg32(int hwreg) {
    int reg = HWRegisters[hwreg].private;

    if (iRegs[reg].reg == -1) {
        SysPrintf("error: flushing unmapped psx register");
    }

    if (HWRegisters[hwreg].usage & HWUSAGE_WRITE) {
        if (branch) {
            /*int reguse = nextPsxRegUse(pc-8, reg);
            if (reguse == REGUSE_NONE || (reguse & REGUSE_READ))*/
            {
                STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
            }
        } else {
            int reguse = nextPsxRegUse(pc - 4, reg);
            if (reguse == REGUSE_NONE || (reguse & REGUSE_READ)) {
                STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
            }
        }
    }

    iRegs[reg].reg = -1;
    iRegs[reg].state = ST_UNK;
}

int GetHWReg32(int reg) {
    int usage = HWUSAGE_PSXREG | HWUSAGE_READ;

    if (reg == 0) {
        return GetHWRegSpecial(REG_RZERO);
    }
    if (!IsMapped(reg)) {
        usage |= HWUSAGE_INITED;
        MapPsxReg32(reg);

        HWRegisters[iRegs[reg].reg].usage |= HWUSAGE_RESERVED;
        if (IsConst(reg)) {
            LIW(HWRegisters[iRegs[reg].reg].code, iRegs[reg].k);
            usage |= HWUSAGE_WRITE | HWUSAGE_CONST;
            //iRegs[reg].state &= ~ST_CONST;
        } else {
            LWZ(HWRegisters[iRegs[reg].reg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
        }
        HWRegisters[iRegs[reg].reg].usage &= ~HWUSAGE_RESERVED;
    }

    return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

int PutHWReg32(int reg) {
    int usage = HWUSAGE_PSXREG | HWUSAGE_WRITE;
    if (reg == 0) {
        return PutHWRegSpecial(REG_WZERO);
    }

    if (!IsMapped(reg)) {
        usage |= HWUSAGE_INITED;
        MapPsxReg32(reg);
    }

    iRegs[reg].state &= ~ST_CONST;

    return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

/* --- Special register mapping --- */

int GetSpecialIndexFromHWRegs(int which) {
    int i;
    for (i = 0; i < NUM_HW_REGISTERS; i++) {
        if (HWRegisters[i].usage & HWUSAGE_SPECIAL) {
            if (HWRegisters[i].private == which) {
                return i;
            }
        }
    }
    return -1;
}

int MapRegSpecial(int which) {
    int hwreg = GetFreeHWReg();
    HWRegisters[hwreg].flush = FlushRegSpecial;
    HWRegisters[hwreg].private = which;

    return hwreg;
}

void FlushRegSpecial(int hwreg) {
    int which = HWRegisters[hwreg].private;

    if (!(HWRegisters[hwreg].usage & HWUSAGE_WRITE))
        return;

    switch (which) {
        case CYCLECOUNT:
            STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.cycle), GetHWRegSpecial(PSXREGS));
            break;
        case PSXPC:
            STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.pc), GetHWRegSpecial(PSXREGS));
            break;
        case TARGET:
            STW(HWRegisters[hwreg].code, 0, GetHWRegSpecial(TARGETPTR));
            break;
    }
}

int GetHWRegSpecial(int which) {
    int index = GetSpecialIndexFromHWRegs(which);
    int usage = HWUSAGE_READ | HWUSAGE_SPECIAL;

    if (index == -1) {
        usage |= HWUSAGE_INITED;
        index = MapRegSpecial(which);

        HWRegisters[index].usage |= HWUSAGE_RESERVED;
        switch (which) {
            case PSXREGS:
            case PSXMEM:
                SysPrintf("error! shouldn't be here!\n");
                //HWRegisters[index].flush = NULL;
                //LIW(HWRegisters[index].code, (u32)&psxRegs);
                break;
            case TARGETPTR:
                HWRegisters[index].flush = NULL;
                LIW(HWRegisters[index].code, (u32) & target);
                break;
            case REG_RZERO:
                HWRegisters[index].flush = NULL;
                LIW(HWRegisters[index].code, 0);
                break;
            case CYCLECOUNT:
                LWZ(HWRegisters[index].code, OFFSET(&psxRegs, &psxRegs.cycle), GetHWRegSpecial(PSXREGS));
                break;
            case PSXPC:
                LWZ(HWRegisters[index].code, OFFSET(&psxRegs, &psxRegs.pc), GetHWRegSpecial(PSXREGS));
                break;
            case TARGET:
                LWZ(HWRegisters[index].code, 0, GetHWRegSpecial(TARGETPTR));
                break;
            default:
                SysPrintf("Error: Unknown special register in GetHWRegSpecial()\n");
                break;
        }
        HWRegisters[index].usage &= ~HWUSAGE_RESERVED;
    }

    return UpdateHWRegUsage(index, usage);
}

int PutHWRegSpecial(int which) {
    int index = GetSpecialIndexFromHWRegs(which);
    int usage = HWUSAGE_WRITE | HWUSAGE_SPECIAL;

    switch (which) {
        case PSXREGS:
        case TARGETPTR:
            SysPrintf("Error: Read-only special register in PutHWRegSpecial()\n");
        case REG_WZERO:
            if (index >= 0) {
                if (HWRegisters[index].usage & HWUSAGE_WRITE)
                    break;
            }
            index = MapRegSpecial(which);
            HWRegisters[index].flush = NULL;
            break;
        default:
            if (index == -1) {
                usage |= HWUSAGE_INITED;
                index = MapRegSpecial(which);

                HWRegisters[index].usage |= HWUSAGE_RESERVED;
            }
            HWRegisters[index].usage &= ~HWUSAGE_RESERVED;
            break;
    }

    return UpdateHWRegUsage(index, usage);
}

static void MapConst(int reg, u32 _const) {
    if (reg == 0)
        return;
    if (IsConst(reg) && iRegs[reg].k == _const)
        return;

    DisposeHWReg(iRegs[reg].reg);
    iRegs[reg].k = _const;
    iRegs[reg].state = ST_CONST;
}

static void MapCopy(int dst, int src) {
    // do it the lazy way for now
    MR(PutHWReg32(dst), GetHWReg32(src));
}

static void iFlushReg(u32 nextpc, int reg) {
    if (!IsMapped(reg) && IsConst(reg)) {
        GetHWReg32(reg);
    }
    if (IsMapped(reg)) {
        if (nextpc) {
            int use = nextPsxRegUse(nextpc, reg);
            if ((use & REGUSE_RW) == REGUSE_WRITE) {
                DisposeHWReg(iRegs[reg].reg);
            } else {
                FlushHWReg(iRegs[reg].reg);
            }
        } else {
            FlushHWReg(iRegs[reg].reg);
        }
    }
}

static void iFlushRegs(u32 nextpc) {
    int i;

    for (i = 1; i < NUM_REGISTERS; i++) {
        iFlushReg(nextpc, i);
    }
}

static void Return() {
    iFlushRegs(0);
    FlushAllHWReg();
    if (((u32)returnPC & 0x1fffffc) == (u32)returnPC) {
            BA((u32)returnPC);
    }
    else {
            LIW(0, (u32)returnPC);
            MTLR(0);
            BLR();
    }
}

static void iRet() {
    /* store cycle */
    count = ((pc - pcold) / 4) * BIAS;
    ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
    Return();
}

static int iLoadTest() {
    u32 tmp;

    // check for load delay
    tmp = psxRegs.code >> 26;
    switch (tmp) {
        case 0x10: // COP0
            switch (_Rs_) {
                case 0x00: // MFC0
                case 0x02: // CFC0
                    return 1;
            }
            break;
        case 0x12: // COP2
            switch (_Funct_) {
                case 0x00:
                    switch (_Rs_) {
                        case 0x00: // MFC2
                        case 0x02: // CFC2
                            return 1;
                    }
                    break;
            }
            break;
        case 0x32: // LWC2
            return 1;
        default:
            if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
                return 1;
            }
            break;
    }
    return 0;
}

/* set a pending branch */
static void SetBranch() {
    int treg;
    branch = 1;
    psxRegs.code = PSXMu32(pc);
    pc += 4;

    if (iLoadTest() == 1) {
        iFlushRegs(0);
        LIW(0, psxRegs.code);
        STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));
        
        /* store cycle */
        count = ((pc - pcold) / 4) * BIAS;
        ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);
        
        treg = GetHWRegSpecial(TARGET);
        MR(4, treg);
        DisposeHWReg(GetHWRegFromCPUReg(treg));
        LIW(3, _Rt_);
        //LIW(GetHWRegSpecial(PSXPC), pc);
        FlushAllHWReg();
        CALLFunc((u32) psxDelayTest);
        FlushAllHWReg();
        // ADD32ItoR(ESP, 2*4);

        Return();
        return;
    }

	recBSC[psxRegs.code>>26]();

    iFlushRegs(0);
    treg = GetHWRegSpecial(TARGET);
    MR(PutHWRegSpecial(PSXPC), treg); // FIXME: this line should not be needed
    DisposeHWReg(GetHWRegFromCPUReg(treg));
    FlushAllHWReg();

    CALLFunc((u32) psxBranchTest);

    iRet();
}

static void iJump(u32 branchPC) {
    branch = 1;
    psxRegs.code = PSXMu32(pc);    

    if (iLoadTest() == 1) {
        iFlushRegs(0);
        LIW(0, psxRegs.code);
        STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));
        /* store cycle */
        count = ((pc - pcold) / 4) * BIAS;
        ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);

        LIW(4, branchPC);
        LIW(3, _Rt_);
        //LIW(GetHWRegSpecial(PSXPC), pc);
        FlushAllHWReg();
        CALLFunc((u32) psxDelayTest);

        //ADD32ItoR(ESP, 2 * 4);
        
        Return();
        return;
    }

	pc += 4;
	
    recBSC[psxRegs.code >> 26]();

    iFlushRegs(branchPC);
    LIW(PutHWRegSpecial(PSXPC), branchPC);
    FlushAllHWReg();

    CALLFunc((u32) psxBranchTest);

    /* store cycle */
    //FlushAllHWReg();
    count = ((pc - pcold) / 4) * BIAS;
    ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);

    // always return for now...
    Return();
}

static void iBranch(u32 branchPC, int savectx) {
    HWRegister HWRegistersS[NUM_HW_REGISTERS];
    iRegisters iRegsS[NUM_REGISTERS];
    int HWRegUseCountS = 0;

    if (savectx) {
        memcpy(iRegsS, iRegs, sizeof (iRegs));
        memcpy(HWRegistersS, HWRegisters, sizeof (HWRegisters));
        HWRegUseCountS = HWRegUseCount;
    }

    branch = 1;
    psxRegs.code = PSXMu32(pc);

    // the delay test is only made when the branch is taken
    // savectx == 0 will mean that :)
    if (savectx == 0 && iLoadTest() == 1) {
        iFlushRegs(0);
        LIW(0, psxRegs.code);
        STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS));
        /* store cycle */
        count = (((pc + 4) - pcold) / 4) * BIAS;
        ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);

        LIW(4, branchPC);
        LIW(3, _Rt_);
        // LIW(GetHWRegSpecial(PSXPC), pc);
        FlushAllHWReg();
        CALLFunc((u32) psxDelayTest);
        //ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), 8);
        Return();
        return;
    }

    pc += 4;
    recBSC[psxRegs.code >> 26]();

    iFlushRegs(branchPC);
    LIW(PutHWRegSpecial(PSXPC), branchPC);
    FlushAllHWReg();

    CALLFunc((u32) psxBranchTest);

    /* store cycle */
    //FlushAllHWReg();
    count = ((pc - pcold) / 4) * BIAS;
    ADDI(PutHWRegSpecial(CYCLECOUNT), GetHWRegSpecial(CYCLECOUNT), count);

	// always return for now...
    Return();

    pc -= 4;
    if (savectx) {
        memcpy(iRegs, iRegsS, sizeof (iRegs));
        memcpy(HWRegisters, HWRegistersS, sizeof (HWRegisters));
        HWRegUseCount = HWRegUseCountS;
    }
}

#define REC_FUNC_R8(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(3, (u32)psxRegs.code); \
	STW(3, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
	FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
}

#define REC_FUNC(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(3, (u32)psxRegs.code); \
	STW(3, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
	FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
}

#define REC_SYS(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(3, (u32)psxRegs.code); \
        STW(3, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
        FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
	branch = 2; \
	iRet(); \
}

#define REC_BRANCH(f) \
void psx##f(); \
static void rec##f() { \
	iFlushRegs(0); \
        LIW(3, (u32)psxRegs.code); \
        STW(3, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
        LIW(PutHWRegSpecial(PSXPC), (u32)pc); \
        FlushAllHWReg(); \
	CALLFunc((u32)psx##f); \
	branch = 2; \
	iRet(); \
}

#define CP2_FUNC(f) \
void gte##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	LIW(0, (u32)psxRegs.code); \
	STW(0, OFFSET(&psxRegs, &psxRegs.code), GetHWRegSpecial(PSXREGS)); \
	FlushAllHWReg(); \
	CALLFunc ((u32)gte##f); \
}

#define CP2_FUNCNC(f) \
void gte##f(); \
static void rec##f() { \
	iFlushRegs(0); \
	CALLFunc ((u32)gte##f); \
/*	branch = 2; */\
}

static int allocMem() {
    int i;

    for (i = 0; i < 0x80; i++) psxRecLUT[i + 0x0000] = (u32) & recRAM[(i & 0x1f) << 16];
    memcpy(psxRecLUT + 0x8000, psxRecLUT, 0x80 * 4);
    memcpy(psxRecLUT + 0xa000, psxRecLUT, 0x80 * 4);

    for (i = 0; i < 0x08; i++) psxRecLUT[i + 0xbfc0] = (u32) & recROM[i << 16];

    return 0;
}

int recInit() {
    
	//recInitDynaMemVM();
    return allocMem();
}

void recReset() {
    printf("recReset ..\r\n");
    memset(recRAM, 0, 0x200000);
    memset(recROM, 0, 0x080000);

    ppcInit();
    ppcSetPtr((u32 *) recMem);

    branch = 0;
    memset(iRegs, 0, sizeof (iRegs));
    iRegs[0].state = ST_CONST;
    iRegs[0].k = 0;
}

static void recShutdown() {
	//recDestroyDynaMemVM();
	
}

static void recError() {
    SysReset();
    ClosePlugins();
    SysMessage("Unrecoverable error while running recompiler\n");
    SysRunGui();
}

__inline static void execute() {
    void (**recFunc)();
    char *p;

    p = (char*) PC_REC(psxRegs.pc);

    recFunc = (void (**)()) (u32) p;

    if (*recFunc == 0) {
        recRecompile();
    }
    recRun(*recFunc, (u32) & psxRegs, (u32) & psxM);
}

void recExecute() {
    while (1) 
        execute();
}

void recExecuteBlock() {
    execute();
}

void recClear(u32 Addr, u32 Size) {
    //printf("recClear\r\n");
    memset((void*) PC_REC(Addr), 0, Size * 4);
}

static void recNULL() {
    //	SysMessage("recUNK: %8.8x\n", psxRegs.code);
}

/*********************************************************
 * goes to opcodes tables...                              *
 * Format:  table[something....]                          *
 *********************************************************/

//REC_SYS(SPECIAL);

static void recSPECIAL() {
    recSPC[_Funct_]();
}

static void recREGIMM() {
    recREG[_Rt_]();
}

static void recCOP0() {
    recCP0[_Rs_]();
}

//REC_SYS(COP2);

static void recCOP2() {
    recCP2[_Funct_]();
}

static void recBASIC() {
    recCP2BSC[_Rs_]();
}

//end of Tables opcodes...

/* - Arithmetic with immediate operand - */

/*********************************************************
 * Arithmetic with immediate operand                      *
 * Format:  OP rt, rs, immediate                          *
 *********************************************************/

static void recADDIU() {
    // Rt = Rs + Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k + _Imm_);
    } else {
        if (_Imm_ == 0) {
            MapCopy(_Rt_, _Rs_);
        } else {
            ADDI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _Imm_);
        }
    }
}

static void recADDI() {
    // Rt = Rs + Im
    recADDIU();
}

//CR0:	SIGN      | POSITIVE | ZERO  | SOVERFLOW | SOVERFLOW | OVERFLOW | CARRY

static void recSLTI() {
    // Rt = Rs < Im (signed)
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, (s32) iRegs[_Rs_].k < _Imm_);
    } else {
        if (_Imm_ == 0) {
            SRWI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), 31);
        } else {
            int reg;
            CMPWI(GetHWReg32(_Rs_), _Imm_);
            reg = PutHWReg32(_Rt_);
            LI(reg, 1);
            BLT(1);
            LI(reg, 0);
        }
    }
}

static void recSLTIU() {
    // Rt = Rs < Im (unsigned)
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k < _ImmU_);
    } else {
        int reg;
        CMPLWI(GetHWReg32(_Rs_), _Imm_);
        reg = PutHWReg32(_Rt_);
        LI(reg, 1);
        BLT(1);
        LI(reg, 0);
    }
}

static void recANDI() {
    // Rt = Rs And Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k & _ImmU_);
    } else {
        ANDI_(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
    }
}

static void recORI() {
    // Rt = Rs Or Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k | _ImmU_);
    } else {
        if (_Imm_ == 0) {
            MapCopy(_Rt_, _Rs_);
        } else {
            ORI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
        }
    }
}

static void recXORI() {
    // Rt = Rs Xor Im
    if (!_Rt_) return;

    if (IsConst(_Rs_)) {
        MapConst(_Rt_, iRegs[_Rs_].k ^ _ImmU_);
    } else {
        XORI(PutHWReg32(_Rt_), GetHWReg32(_Rs_), _ImmU_);
    }
}

//end of * Arithmetic with immediate operand  

/*********************************************************
 * Load higher 16 bits of the first word in GPR with imm  *
 * Format:  OP rt, immediate                              *
 *********************************************************/

static void recLUI() {
    // Rt = Imm << 16
    if (!_Rt_) return;

    MapConst(_Rt_, _Imm_ << 16);
}

//End of Load Higher .....

/* - Register arithmetic - */

/*********************************************************
 * Register arithmetic                                    *
 * Format:  OP rd, rs, rt                                 *
 *********************************************************/

static void recADDU() {
    // Rd = Rs + Rt 
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k + iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        if ((s32) (s16) iRegs[_Rs_].k == (s32) iRegs[_Rs_].k) {
            ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), (s16) iRegs[_Rs_].k);
        } else if ((iRegs[_Rs_].k & 0xffff) == 0) {
            ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k >> 16);
        } else {
            ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((s32) (s16) iRegs[_Rt_].k == (s32) iRegs[_Rt_].k) {
            ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), (s16) iRegs[_Rt_].k);
        } else if ((iRegs[_Rt_].k & 0xffff) == 0) {
            ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k >> 16);
        } else {
            ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        ADD(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recADD() {
    // Rd = Rs + Rt
    recADDU();
}

static void recSUBU() {
    // Rd = Rs - Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k - iRegs[_Rt_].k);
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((s32) (s16) (-iRegs[_Rt_].k) == (s32) (-iRegs[_Rt_].k)) {
            ADDI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), -iRegs[_Rt_].k);
        } else if (((-iRegs[_Rt_].k) & 0xffff) == 0) {
            ADDIS(PutHWReg32(_Rd_), GetHWReg32(_Rs_), (-iRegs[_Rt_].k) >> 16);
        } else {
            SUB(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        SUB(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recSUB() {
    // Rd = Rs - Rt
    recSUBU();
}

static void recAND() {
    // Rd = Rs And Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k & iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        // TODO: implement shifted (ANDIS) versions of these
        if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
            ANDI_(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
        } else {
            AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
            ANDI_(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
        } else {
            AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        AND(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recOR() {
    // Rd = Rs Or Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k | iRegs[_Rt_].k);
    } else {
        if (_Rs_ == _Rt_) {
            MapCopy(_Rd_, _Rs_);
        } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
            if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
                ORI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else {
                OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
            if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
                ORI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else {
                OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else {
            OR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    }
}

static void recXOR() {
    // Rd = Rs Xor Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k ^ iRegs[_Rt_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
            XORI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
        } else {
            XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
        if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
            XORI(PutHWReg32(_Rd_), GetHWReg32(_Rs_), iRegs[_Rt_].k);
        } else {
            XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }
    } else {
        XOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recNOR() {
// Rd = Rs Nor Rt
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
            MapConst(_Rd_, ~(iRegs[_Rs_].k | iRegs[_Rt_].k));
    } else if (IsConst(_Rs_)) {
            LI(0, iRegs[_Rs_].k);
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rt_), 0);
    } else if (IsConst(_Rt_)) {
            LI(0, iRegs[_Rt_].k);
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), 0);
    } else {
            NOR(PutHWReg32(_Rd_), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    }
}

static void recSLT() {
    // Rd = Rs < Rt (signed)
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, (s32) iRegs[_Rs_].k < (s32) iRegs[_Rt_].k);
    } else { // TODO: add immidiate cases
        int reg;
        CMPW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        reg = PutHWReg32(_Rd_);
        LI(reg, 1);
        BLT(1);
        LI(reg, 0);
    }
}

static void recSLTU() {
    // Rd = Rs < Rt (unsigned)
    if (!_Rd_) return;

    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rs_].k < iRegs[_Rt_].k);
    } else { // TODO: add immidiate cases
        SUBFC(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
        SUBFE(PutHWReg32(_Rd_), GetHWReg32(_Rd_), GetHWReg32(_Rd_));
        NEG(PutHWReg32(_Rd_), GetHWReg32(_Rd_));
    }
}

//End of * Register arithmetic

/* - mult/div & Register trap logic - */

/*********************************************************
 * Register mult/div & Register trap logic                *
 * Format:  OP rs, rt                                     *
 *********************************************************/

int DoShift(u32 k) {
    u32 i;
    for (i = 0; i < 30; i++) {
        if (k == (1ul << i))
            return i;
    }
    return -1;
}

static void recMULT() {
    // Lo/Hi = Rs * Rt (signed)
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        u64 res = (s64) ((s64) (s32) iRegs[_Rs_].k * (s64) (s32) iRegs[_Rt_].k);
        MapConst(REG_LO, (res & 0xffffffff));
        MapConst(REG_HI, ((res >> 32) & 0xffffffff));
        return;
    }
	MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
	MULHW(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
}

static void recMULTU() {
    if (IsConst(_Rs_) && IsConst(_Rt_)) {
        u64 res = (u64) ((u64) (u32) iRegs[_Rs_].k * (u64) (u32) iRegs[_Rt_].k);
        MapConst(REG_LO, (res & 0xffffffff));
        MapConst(REG_HI, ((res >> 32) & 0xffffffff));
        return;
    }

	MULLW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
	MULHWU(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
}


#if 0
static void recDIV() {
    // Lo/Hi = Rs / Rt (signed)
    int usehi;

    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (s32) iRegs[_Rs_].k / (s32) iRegs[_Rt_].k);
        MapConst(REG_HI, (s32) iRegs[_Rs_].k % (s32) iRegs[_Rt_].k);
        return;
    }

    usehi = isPsxRegUsed(pc, REG_HI);

    if (IsConst(_Rt_)) {
        int shift = DoShift(iRegs[_Rt_].k);
        if (shift != -1) {
            SRAWI(PutHWReg32(REG_LO), GetHWReg32(_Rs_), shift);
            ADDZE(PutHWReg32(REG_LO), GetHWReg32(REG_LO));
            if (usehi) {
                RLWINM(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 0, (31 - shift), 31);
            }
        } else if (iRegs[_Rt_].k == 3) {
            // http://the.wall.riscom.net/books/proc/ppc/cwg/code2.html
            LIS(PutHWReg32(REG_HI), 0x5555);
            ADDI(PutHWReg32(REG_HI), GetHWReg32(REG_HI), 0x5556);
            MULHW(PutHWReg32(REG_LO), GetHWReg32(REG_HI), GetHWReg32(_Rs_));
            SRWI(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 31);
            ADD(PutHWReg32(REG_LO), GetHWReg32(REG_LO), GetHWReg32(REG_HI));
            if (usehi) {
                MULLI(PutHWReg32(REG_HI), GetHWReg32(REG_LO), 3);
                SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
            }
        } else {
            DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            if (usehi) {
                if ((iRegs[_Rt_].k & 0x7fff) == iRegs[_Rt_].k) {
                    MULLI(PutHWReg32(REG_HI), GetHWReg32(REG_LO), iRegs[_Rt_].k);
                } else {
                    MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
                }
                SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
            }
        }
    } else {
        DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        if (usehi) {
            MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
            SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
        }
    }
}

static void recDIVU() {
    // Lo/Hi = Rs / Rt (unsigned)
    int usehi;

    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (u32) iRegs[_Rs_].k / (u32) iRegs[_Rt_].k);
        MapConst(REG_HI, (u32) iRegs[_Rs_].k % (u32) iRegs[_Rt_].k);
        return;
    }

    usehi = isPsxRegUsed(pc, REG_HI);

    if (IsConst(_Rt_)) {
        int shift = DoShift(iRegs[_Rt_].k);
        if (shift != -1) {
            SRWI(PutHWReg32(REG_LO), GetHWReg32(_Rs_), shift);
            if (usehi) {
                RLWINM(PutHWReg32(REG_HI), GetHWReg32(_Rs_), 0, (31 - shift), 31);
            }
        } else {
            DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            if (usehi) {
                MULLW(PutHWReg32(REG_HI), GetHWReg32(_Rt_), GetHWReg32(REG_LO));
                SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
            }
        }
    } else {
        DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        if (usehi) {
            MULLW(PutHWReg32(REG_HI), GetHWReg32(_Rt_), GetHWReg32(REG_LO));
            SUB(PutHWReg32(REG_HI), GetHWReg32(_Rs_), GetHWReg32(REG_HI));
        }
    }
}
#elif 1 // + 2 fps in castlevania opening
static void recDIV() {
    // Lo/Hi = Rs / Rt (signed)
    int usehi;

    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (s32) iRegs[_Rs_].k / (s32) iRegs[_Rt_].k);
        MapConst(REG_HI, (s32) iRegs[_Rs_].k % (s32) iRegs[_Rt_].k);
        return;
    }
    
    usehi = isPsxRegUsed(pc, REG_HI);
    
    DIVW(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
    if (usehi) {
		MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
		SUBF(PutHWReg32(REG_HI), GetHWReg32(REG_HI), GetHWReg32(_Rs_));
	}
}

static void recDIVU() {
    // Lo/Hi = Rs / Rt (unsigned)
    int usehi;
     
    if (IsConst(_Rs_) && iRegs[_Rs_].k == 0) {
        MapConst(REG_LO, 0);
        MapConst(REG_HI, 0);
        return;
    }
    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(REG_LO, (u32) iRegs[_Rs_].k / (u32) iRegs[_Rt_].k);
        MapConst(REG_HI, (u32) iRegs[_Rs_].k % (u32) iRegs[_Rt_].k);
        return;
    }
    
    usehi = isPsxRegUsed(pc, REG_HI);
    
	DIVWU(PutHWReg32(REG_LO), GetHWReg32(_Rs_), GetHWReg32(_Rt_));
	if (usehi) {
		MULLW(PutHWReg32(REG_HI), GetHWReg32(REG_LO), GetHWReg32(_Rt_));
		SUBF(PutHWReg32(REG_HI), GetHWReg32(REG_HI), GetHWReg32(_Rs_));
	}
}
#else
REC_FUNC(DIV);
REC_FUNC(DIVU);
#endif
//End of * Register mult/div & Register trap logic  

static u32 sioRead32() {
	u32 hard;
	hard = sioRead8();
	hard |= sioRead8() << 8;
	hard |= sioRead8() << 16;
	hard |= sioRead8() << 24;
	return hard;
}

static u16 sioRead16() {
	u16 hard;
	hard = sioRead8();
	hard |= sioRead8() << 8;
	return hard;
}

REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(SWL);
REC_FUNC(SWR);


static void preMemRead() {
    if (_Rs_ != _Rt_) {
        DisposeHWReg(iRegs[_Rt_].reg);
    }
    ADDI(3, GetHWReg32(_Rs_), _Imm_);
    if (_Rs_ == _Rt_) {
        DisposeHWReg(iRegs[_Rt_].reg);
    }
    InvalidateCPURegs();
}

static void preMemWrite(int size) {
    //ReserveArgs(2);
    ADDI(3, GetHWReg32(_Rs_), _Imm_);

    switch(size)
    {
        case 1:
            RLWINM(4, GetHWReg32(_Rt_), 0, 24, 31);
            break;
        case 2:
            RLWINM(4, GetHWReg32(_Rt_), 0, 16, 31);
            break;
        default:
            MR(4, GetHWReg32(_Rt_));
    }

    InvalidateCPURegs();
}

#if 1
static void recLB() {

	preMemRead();
	CALLFunc((u32) psxMemRead8);
	if (_Rt_) {
		EXTSB(PutHWReg32(_Rt_), 3);
	}
}

static void recLBU() {
	
	preMemRead();
	CALLFunc((u32) psxMemRead8);

	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
}

static void recLH() {
	
	preMemRead();
	CALLFunc((u32) psxMemRead16);
	if (_Rt_) {
		EXTSH(PutHWReg32(_Rt_), 3);
	}
}

static void recLHU() {
	
	preMemRead();
	CALLFunc((u32) psxMemRead16);
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
}

static void recLW() {

	preMemRead();
	CALLFunc((u32) psxMemRead32);
	if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
	}
}
static void recSB() {

	preMemWrite(1);
	CALLFunc((u32) psxMemWrite8);
}

static void recSH() {

	preMemWrite(2);
	CALLFunc((u32) psxMemWrite16);
}

static void recSW() {

	preMemWrite(4);
	CALLFunc((u32) psxMemWrite32);
}
#elif 1

static void recLB() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_LB,_Imm_);
}

static void recLBU() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_LBU,_Imm_);
}

static void recLH() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_LH,_Imm_);
}

static void recLHU() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_LHU,_Imm_);
}

static void recLW() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_LW,_Imm_);
}
static void recSB() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_SB,_Imm_);
}

static void recSH() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_SH,_Imm_);
}

static void recSW() {
	recCallDynaMemVM(_Rs_,_Rt_,MEM_SW,_Imm_);
}
#else // Call interpreted function

static void recSB() {
    preMemWrite(1);
    CALLFunc((u32) psxMemWrite8);
}

static void recSH() {
    preMemWrite(2);
    CALLFunc((u32) psxMemWrite16);
}

static void recSW() {
	// mem[Rs + Im] = Rt
    preMemWrite(4);
    CALLFunc((u32) psxMemWrite32);
}

#if 1
static void recLB() {
    preMemRead();
    CALLFunc((u32) psxMemRead8);
    if (_Rt_) {
        EXTSB(PutHWReg32(_Rt_), 3);
    }
}

static void recLBU() {
    preMemRead();
    CALLFunc((u32) psxMemRead8);

    if (_Rt_) {
        MR(PutHWReg32(_Rt_),3);
    }
}

static void recLH() {
    preMemRead();
    CALLFunc((u32) psxMemRead16);
    if (_Rt_) {
        EXTSH(PutHWReg32(_Rt_), 3);
    }
}

static void recLHU() {
    preMemRead();
    CALLFunc((u32) psxMemRead16);
    if (_Rt_) {
	    MR(PutHWReg32(_Rt_),3);
    }
}
static void recLW() {

    preMemRead();
    CALLFunc((u32) psxMemRead32);
    if (_Rt_) {
	    MR(PutHWReg32(_Rt_),3);
    }
}
#else // rewrote func
static void recLB() {
    // Rt = mem[Rs + Im] (signed)
#if 1
    if (IsConst(_Rs_)) {
        u32 addr = iRegs[_Rs_].k + _Imm_;
        int t = addr >> 16;

        if ((t & 0xfff0) == 0xbfc0) {
            if (!_Rt_) return;
            // since bios is readonly it won't change
            MapConst(_Rt_, psxRs8(addr));
            return;
        }
        if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxM[addr & 0x1fffff]);
            LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80 && addr < 0x1f801000) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
            LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
            return;
        }
		if (t == 0x1f80) {
            
            switch (addr) {
				case 0x1f801040:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioRead8);

                    MR(PutHWReg32(_Rt_),3);
					EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;

				// sioReadStat16
				case 0x1f801800:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) cdrRead0);

                    MR(PutHWReg32(_Rt_),3);
					EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;	

				// sioReadMode16
				case 0x1f801801:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) cdrRead1);

                    MR(PutHWReg32(_Rt_),3);
					EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;	

				// sioReadCtrl16
				case 0x1f801802:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) cdrRead2);

					MR(PutHWReg32(_Rt_),3);
					EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;

				// sioReadBaud16
				case 0x1f801803:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) cdrRead3);

                    MR(PutHWReg32(_Rt_),3);
					EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;
				default:
					 if (!_Rt_) return;

					LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
					LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
					EXTSB(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
					return;
			}
		}
        //	SysPrintf("unhandled r8 %x\n", addr);
    }
#endif
    preMemRead();
    CALLFunc((u32) psxMemRead8);
    if (_Rt_) {
        EXTSB(PutHWReg32(_Rt_), 3);
    }
}

static void recLBU() {
    // Rt = mem[Rs + Im] (unsigned)
#if 1
    if (IsConst(_Rs_)) {
        u32 addr = iRegs[_Rs_].k + _Imm_;
        int t = addr >> 16;

        if ((t & 0xfff0) == 0xbfc0) {
            if (!_Rt_) return;
            // since bios is readonly it won't change
            MapConst(_Rt_, psxRu8(addr));
            return;
        }
        if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxM[addr & 0x1fffff]);
            LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80 && addr < 0x1f801000) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
            LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            return;
        }
		if (t == 0x1f80) {

			switch (addr) {
				case 0x1f801040:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) sioRead8);

					MR(PutHWReg32(_Rt_),3);
					return;

				// sioReadStat16
				case 0x1f801800:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) cdrRead0);

					MR(PutHWReg32(_Rt_),3);
					return;	

				// sioReadMode16
				case 0x1f801801:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) cdrRead1);

					MR(PutHWReg32(_Rt_),3);
					return;	

				// sioReadCtrl16
				case 0x1f801802:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) cdrRead2);

					MR(PutHWReg32(_Rt_),3);
					return;

				// sioReadBaud16
				case 0x1f801803:
					if (!_Rt_) return;

					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) cdrRead3);

					MR(PutHWReg32(_Rt_),3);
					return;
				default:
					if (!_Rt_) return;

					LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
					LBZ(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
					return;
			}
		}
        //	SysPrintf("unhandled r8 %x\n", addr);
    }
#endif
    preMemRead();
    CALLFunc((u32) psxMemRead8);

    if (_Rt_) {
        MR(PutHWReg32(_Rt_),3);
    }
}

static void recLH() {
    // Rt = mem[Rs + Im] (signed)
#if 1
    if (IsConst(_Rs_)) {
        u32 addr = iRegs[_Rs_].k + _Imm_;
        int t = addr >> 16;

        if ((t & 0xfff0) == 0xbfc0) {
            if (!_Rt_) return;
            // since bios is readonly it won't change
            MapConst(_Rt_, psxRs16(addr));
            return;
        }
        if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxM[addr & 0x1fffff]);
            LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80 && addr < 0x1f801000) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
            LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
            return;
        }
		if (t == 0x1f80) {
            
            switch (addr) {		

				// sioWrite16
				case 0x1f801040:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioRead16);

                    MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;

				// sioReadStat16
				case 0x1f801044:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadStat16);

                    MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;	

				// sioReadMode16
				case 0x1f801048:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadMode16);

                    MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;	

				// sioReadCtrl16
				case 0x1f80104a:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadCtrl16);

                    MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;

				// sioReadBaud16
				case 0x1f80104e:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadBaud16);

                    MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;

                case 0x1f801100: case 0x1f801110: case 0x1f801120:
                    if (!_Rt_) return;

                    LIW(3, (addr >> 4) & 0x3);
                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) psxRcntRcount);

					MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    
                    return;

                case 0x1f801104: case 0x1f801114: case 0x1f801124:
                    if (!_Rt_) return;

                    LIW(3, (addr >> 4) & 0x3);
                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) psxRcntRmode);

                    MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    
                    return;

                case 0x1f801108: case 0x1f801118: case 0x1f801128:
                    if (!_Rt_) return;

                    LIW(3, (addr >> 4) & 0x3);
                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) psxRcntRtarget);

					MR(PutHWReg32(_Rt_),3);
					EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
                    return;

				default:
					if (addr >= 0x1f801c00 && addr < 0x1f801e00) {
						if (!_Rt_) return;

						LIW(3, addr);
						DisposeHWReg(iRegs[_Rt_].reg);
						InvalidateCPURegs();
						CALLFunc((u32) SPU_readRegister);
						
						MR(PutHWReg32(_Rt_),3);
						EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
						return;
					}
					else{
						LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
						LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
						EXTSH(PutHWReg32(_Rt_), GetHWReg32(_Rt_));
						return;
					}

            }
		}
        //	SysPrintf("unhandled r16 %x\n", addr);
    }
#endif
    preMemRead();
    CALLFunc((u32) psxMemRead16);
    if (_Rt_) {
        EXTSH(PutHWReg32(_Rt_), 3);
    }
}

static void recLHU() {
    // Rt = mem[Rs + Im] (unsigned)
#if 1
    if (IsConst(_Rs_)) {
        u32 addr = iRegs[_Rs_].k + _Imm_;
        int t = addr >> 16;

        if ((t & 0xfff0) == 0xbfc0) {
            if (!_Rt_) return;
            // since bios is readonly it won't change
            MapConst(_Rt_, psxRu16(addr));
            return;
        }
        if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxM[addr & 0x1fffff]);
            LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80 && addr < 0x1f801000) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
            LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80) {
            
            switch (addr) {		

				// sioWrite16
				case 0x1f801040:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioRead16);

                    MR(PutHWReg32(_Rt_),3);
                    return;

				// sioReadStat16
				case 0x1f801044:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadStat16);

                    MR(PutHWReg32(_Rt_),3);
                    return;	

				// sioReadMode16
				case 0x1f801048:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadMode16);

                    MR(PutHWReg32(_Rt_),3);
                    return;	

				// sioReadCtrl16
				case 0x1f80104a:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadCtrl16);

                    MR(PutHWReg32(_Rt_),3);
                    return;

				// sioReadBaud16
				case 0x1f80104e:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioReadBaud16);

                    MR(PutHWReg32(_Rt_),3);
                    return;

                case 0x1f801100: case 0x1f801110: case 0x1f801120:
                    if (!_Rt_) return;

                    LIW(3, (addr >> 4) & 0x3);
                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) psxRcntRcount);
                    MR(PutHWReg32(_Rt_),3);
                    
                    return;

                case 0x1f801104: case 0x1f801114: case 0x1f801124:
                    if (!_Rt_) return;

                    LIW(3, (addr >> 4) & 0x3);
                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) psxRcntRmode);

                    MR(PutHWReg32(_Rt_),3);
                    
                    return;

                case 0x1f801108: case 0x1f801118: case 0x1f801128:
                    if (!_Rt_) return;

                    LIW(3, (addr >> 4) & 0x3);
                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) psxRcntRtarget);

                    MR(PutHWReg32(_Rt_),3);
                    
                    return;

				default:
					if (addr >= 0x1f801c00 && addr < 0x1f801e00) {
						if (!_Rt_) return;

						LIW(3, addr);
						DisposeHWReg(iRegs[_Rt_].reg);
						InvalidateCPURegs();
						CALLFunc((u32) SPU_readRegister);

						MR(PutHWReg32(_Rt_),3);

						return;
					}
					else{
						LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
						LHBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
						return;
					}

            }
        }
        //	SysPrintf("unhandled r16u %x\n", addr);
    }
#endif
    preMemRead();
    CALLFunc((u32) psxMemRead16);
    if (_Rt_) {
        MR(PutHWReg32(_Rt_),3);
    }
}

static void recLW() {
    // Rt = mem[Rs + Im] (unsigned)
#if 1
    if (IsConst(_Rs_)) {
        u32 addr = iRegs[_Rs_].k + _Imm_;
        int t = addr >> 16;

        if ((t & 0xfff0) == 0xbfc0) {
            if (!_Rt_) return;
            // since bios is readonly it won't change
            MapConst(_Rt_, psxRu32(addr));
            return;
        }
        if ((t & 0x1fe0) == 0 && (t & 0x1fff) != 0) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxM[addr & 0x1fffff]);
            LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80 && addr < 0x1f801000) {
            if (!_Rt_) return;

            LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
            LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
            return;
        }
        if (t == 0x1f80) {
            switch (addr) {
				// sioWrite32
				case 0x1f801040:
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) sioRead32);

                    MR(PutHWReg32(_Rt_),3);
                    return;

				// GPU_readData
                case 0x1f801810:
                    if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) GPU_readData);

                    MR(PutHWReg32(_Rt_),3);
                    return;

				// GPU_readStatus
                case 0x1f801814:
                    if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) GPU_readStatus);

                    MR(PutHWReg32(_Rt_),3);
                    return;


				// mdecRead0()
				case 0x1f801820: 
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) mdecRead0);

                    MR(PutHWReg32(_Rt_),3);
					return;
				// mdecRead1()
				case 0x1f801824: 
					if (!_Rt_) return;

                    DisposeHWReg(iRegs[_Rt_].reg);
                    InvalidateCPURegs();
                    CALLFunc((u32) mdecRead1);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRcount 0
				case 0x1f801100:
					if (!_Rt_) return;

					LIW(3, 0);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRcount);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRmode 0
				case 0x1f801104:
					if (!_Rt_) return;

					LIW(3, 0);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRmode);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRtarget 0
				case 0x1f801108:
					if (!_Rt_) return;

					LIW(3, 0);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRtarget);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRcount 1
				case 0x1f801110:
					if (!_Rt_) return;

					LIW(3, 1);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRcount);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRmode 1
				case 0x1f801114:
					if (!_Rt_) return;

					LIW(3, 1);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRmode);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRtarget 1
				case 0x1f801118:
					if (!_Rt_) return;

					LIW(3, 1);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRtarget);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRcount 2
				case 0x1f801120:
					if (!_Rt_) return;

					LIW(3, 2);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRcount);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRmode 2
				case 0x1f801124:
					if (!_Rt_) return;

					LIW(3, 2);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRmode);

					MR(PutHWReg32(_Rt_),3);
					return;

				// psxRcntRtarget 2
				case 0x1f801128:
					if (!_Rt_) return;

					LIW(3, 2);
					DisposeHWReg(iRegs[_Rt_].reg);
					InvalidateCPURegs();
					CALLFunc((u32) psxRcntRtarget);

					MR(PutHWReg32(_Rt_),3);
					return;					

                default:
                    if (!_Rt_) return;

                    LIW(PutHWReg32(_Rt_), (u32) & psxH[addr & 0xffff]);
                    LWBRX(PutHWReg32(_Rt_), 0, GetHWReg32(_Rt_));
                    return;
            }
        }
        //		SysPrintf("unhandled r32 %x\n", addr);
    }
#endif
    preMemRead();
    CALLFunc((u32) psxMemRead32);
    if (_Rt_) {
		MR(PutHWReg32(_Rt_),3);
    }
}
#endif
#endif

static void recSLL() {
    // Rd = Rt << Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rt_].k << _Sa_);
    } else {
        SLWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

static void recSRL() {
    // Rd = Rt >> Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, iRegs[_Rt_].k >> _Sa_);
    } else {
        SRWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

static void recSRA() {
    // Rd = Rt >> Sa
    if (!_Rd_) return;

    if (IsConst(_Rt_)) {
        MapConst(_Rd_, (s32) iRegs[_Rt_].k >> _Sa_);
    } else {
        SRAWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), _Sa_);
    }
}

/* - shift ops - */
#if 1
REC_FUNC(SLLV);
#else
static void recSLLV() {
    // Rd = Rt << Rs
    if (!_Rd_) return;

    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(_Rd_, iRegs[_Rt_].k << iRegs[_Rs_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        SLWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
    } else {
        SLW(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
    }
}
#endif
#if 1
REC_FUNC(SRLV);
#else
static void recSRLV() {
    // Rd = Rt >> Rs
    if (!_Rd_) return;

    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(_Rd_, iRegs[_Rt_].k >> iRegs[_Rs_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        SRWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
    } else {
        SRW(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
    }
}
#endif
#if 1
REC_FUNC(SRAV);
#else
static void recSRAV() {
    // Rd = Rt >> Rs
    if (!_Rd_) return;

    if (IsConst(_Rt_) && IsConst(_Rs_)) {
        MapConst(_Rd_, (s32) iRegs[_Rt_].k >> iRegs[_Rs_].k);
    } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
        SRAWI(PutHWReg32(_Rd_), GetHWReg32(_Rt_), iRegs[_Rs_].k);
    } else {
        SRAW(PutHWReg32(_Rd_), GetHWReg32(_Rt_), GetHWReg32(_Rs_));
    }
}
#endif
static void recSYSCALL() {
    //	dump=1;
    iFlushRegs(0);

    LIW(PutHWRegSpecial(PSXPC), pc - 4);
    LIW(3, 0x20);
    LIW(4, (branch == 1 ? 1 : 0));
    FlushAllHWReg();
    CALLFunc((u32) psxException);
    
    branch = 2;
    iRet();
}

static void recBREAK() {
}

static void recMFHI() {
    // Rd = Hi
    if (!_Rd_) return;

    if (IsConst(REG_HI)) {
        MapConst(_Rd_, iRegs[REG_HI].k);
    } else {
        MapCopy(_Rd_, REG_HI);
    }
}

static void recMTHI() {
    // Hi = Rs

    if (IsConst(_Rs_)) {
        MapConst(REG_HI, iRegs[_Rs_].k);
    } else {
        MapCopy(REG_HI, _Rs_);
    }
}

static void recMFLO() {
    // Rd = Lo
    if (!_Rd_) return;

    if (IsConst(REG_LO)) {
        MapConst(_Rd_, iRegs[REG_LO].k);
    } else {
        MapCopy(_Rd_, REG_LO);
    }
}

static void recMTLO() {
    // Lo = Rs

    if (IsConst(_Rs_)) {
        MapConst(REG_LO, iRegs[_Rs_].k);
    } else {
        MapCopy(REG_LO, _Rs_);
    }
}

/* - branch ops - */

#if 0
REC_BRANCH(BEQ);     // *FIXME
REC_BRANCH(BNE);     // *FIXME
#else
static void recBEQ() {
    // Branch if Rs == Rt
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (_Rs_ == _Rt_) {
        iJump(bpc);
    } else {
        if (IsConst(_Rs_) && IsConst(_Rt_)) {
            if (iRegs[_Rs_].k == iRegs[_Rt_].k) {
                iJump(bpc);
                return;
            } else {
                iJump(pc + 4);
                return;
            }
        } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
            if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
                CMPLWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else if ((s32) (s16) iRegs[_Rs_].k == (s32) iRegs[_Rs_].k) {
                CMPWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
            if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
                CMPLWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else if ((s32) (s16) iRegs[_Rt_].k == (s32) iRegs[_Rt_].k) {
                CMPWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else {
            CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }

        BEQ_L(b);

        iBranch(pc + 4, 1);

        B_DST(b);

        iBranch(bpc, 0);
        pc += 4;
    }
}

static void recBNE() {
    // Branch if Rs != Rt
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (_Rs_ == _Rt_) {
        iJump(pc + 4);
    } else {
        if (IsConst(_Rs_) && IsConst(_Rt_)) {
            if (iRegs[_Rs_].k != iRegs[_Rt_].k) {
                iJump(bpc);
                return;
            } else {
                iJump(pc + 4);
                return;
            }
        } else if (IsConst(_Rs_) && !IsMapped(_Rs_)) {
            if ((iRegs[_Rs_].k & 0xffff) == iRegs[_Rs_].k) {
                CMPLWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else if ((s32) (s16) iRegs[_Rs_].k == (s32) iRegs[_Rs_].k) {
                CMPWI(GetHWReg32(_Rt_), iRegs[_Rs_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else if (IsConst(_Rt_) && !IsMapped(_Rt_)) {
            if ((iRegs[_Rt_].k & 0xffff) == iRegs[_Rt_].k) {
                CMPLWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else if ((s32) (s16) iRegs[_Rt_].k == (s32) iRegs[_Rt_].k) {
                CMPWI(GetHWReg32(_Rs_), iRegs[_Rt_].k);
            } else {
                CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
            }
        } else {
            CMPLW(GetHWReg32(_Rs_), GetHWReg32(_Rt_));
        }

        BNE_L(b);

        iBranch(pc + 4, 1);

        B_DST(b);

        iBranch(bpc, 0);
        pc += 4;
    }
}
#endif
static void recBLTZ() {
    // Branch if Rs < 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k < 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLT_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBGTZ() {
    // Branch if Rs > 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k > 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGT_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBLTZAL() {
    // Branch if Rs < 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k < 0) {
            MapConst(31, pc + 4);

            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLT_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    MapConst(31, pc + 4);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBGEZAL() {
    // Branch if Rs >= 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k >= 0) {
            MapConst(31, pc + 4);

            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGE_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    MapConst(31, pc + 4);

    iBranch(bpc, 0);
    pc += 4;
}

static void recJ() {
    // j target

    iJump(_Target_ * 4 + (pc & 0xf0000000));
}

static void recJAL() {
    // jal target
    MapConst(31, pc + 4);

    iJump(_Target_ * 4 + (pc & 0xf0000000));
}

static void recJR() {
    // jr Rs

    if (IsConst(_Rs_)) {
        iJump(iRegs[_Rs_].k);
        //LIW(PutHWRegSpecial(TARGET), iRegs[_Rs_].k);
    } else {
        MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
        SetBranch();
    }
}

static void recJALR() {
    // jalr Rs

    if (_Rd_) {
        MapConst(_Rd_, pc + 4);
    }

    if (IsConst(_Rs_)) {
        iJump(iRegs[_Rs_].k);
        //LIW(PutHWRegSpecial(TARGET), iRegs[_Rs_].k);
    } else {
        MR(PutHWRegSpecial(TARGET), GetHWReg32(_Rs_));
        SetBranch();
    }
}


static void recBLEZ() {
    // Branch if Rs <= 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k <= 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BLE_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

static void recBGEZ() {
    // Branch if Rs >= 0
    u32 bpc = _Imm_ * 4 + pc;
    u32 *b;

    if (bpc == pc+4 && psxTestLoadDelay(_Rs_, PSXMu32(bpc)) == 0) {
        return;
    }
    
    if (IsConst(_Rs_)) {
        if ((s32) iRegs[_Rs_].k >= 0) {
            iJump(bpc);
            return;
        } else {
            iJump(pc + 4);
            return;
        }
    }

    CMPWI(GetHWReg32(_Rs_), 0);
    BGE_L(b);

    iBranch(pc + 4, 1);

    B_DST(b);

    iBranch(bpc, 0);
    pc += 4;
}

#if 0
REC_FUNC(MFC0);
REC_SYS(MTC0);
REC_FUNC(CFC0);
REC_SYS(CTC0);
REC_FUNC(RFE);
#else
static void recRFE() {
    
    iFlushRegs(0);
    LWZ(0, OFFSET(&psxRegs, &psxRegs.CP0.n.Status), GetHWRegSpecial(PSXREGS));
    RLWINM(11, 0, 0, 0, 27);
    RLWINM(0, 0, 30, 28, 31);
    OR(0, 0, 11);
    STW(0, OFFSET(&psxRegs, &psxRegs.CP0.n.Status), GetHWRegSpecial(PSXREGS));

    //LIW(PutHWRegSpecial(PSXPC), (u32)pc);
    LIW(PutHWRegSpecial(PSXPC), (u32) pc);
    FlushAllHWReg();
    if (branch == 0) {
        branch = 2;
        iRet();
    }
}

static void recMFC0() {
    // Rt = Cop0->Rd
    if (!_Rt_) return;

    LWZ(PutHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
}

static void recCFC0() {
    // Rt = Cop0->Rd

    recMFC0();
}

static void recMTC0() {
    // Cop0->Rd = Rt
#if 1
    /*if (IsConst(_Rt_)) {
            switch (_Rd_) {
                    case 12:
                            MOV32ItoM((u32)&psxRegs.CP0.r[_Rd_], iRegs[_Rt_].k);
                            break;
                    case 13:
                            MOV32ItoM((u32)&psxRegs.CP0.r[_Rd_], iRegs[_Rt_].k & ~(0xfc00));
                            break;
                    default:
                            MOV32ItoM((u32)&psxRegs.CP0.r[_Rd_], iRegs[_Rt_].k);
                            break;
            }
    } else*/
    {
        switch (_Rd_) {
            case 13:
                RLWINM(0, GetHWReg32(_Rt_), 0, 22, 15); // & ~(0xfc00)
                STW(0, OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
                break;
            default://12
                STW(GetHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]), GetHWRegSpecial(PSXREGS));
                break;
        }
    }

    if (_Rd_ == 12 || _Rd_ == 13) {
        iFlushRegs(0);
        LIW(PutHWRegSpecial(PSXPC), (u32) pc);
        FlushAllHWReg();
        CALLFunc((u32) psxTestSWInts);
// CC        
/*        
        if (_Rd_ == 12) {
            LWZ(0, OFFSET(&psxRegs, &psxRegs.interrupt), GetHWRegSpecial(PSXREGS));
            ORIS(0, 0, 0x8000);
            STW(0, OFFSET(&psxRegs, &psxRegs.interrupt), GetHWRegSpecial(PSXREGS));
        }
*/        
        if (branch == 0) {
            branch = 2;
            iRet();
        }
    }
#else
    
    STW(GetHWReg32(_Rt_), OFFSET(&psxRegs, &psxRegs.CP0.r[_Rd_]),GetHWRegSpecial(PSXREGS));
#endif
}

static void recCTC0() {
    // Cop0->Rd = Rt

    recMTC0();
}
#endif
// GTE function callers
CP2_FUNC(MFC2);
CP2_FUNC(MTC2);
CP2_FUNC(CFC2);
CP2_FUNC(CTC2);
CP2_FUNC(LWC2);
CP2_FUNC(SWC2);
CP2_FUNCNC(RTPS);
CP2_FUNC(OP);
CP2_FUNCNC(NCLIP);
CP2_FUNC(DPCS);
CP2_FUNC(INTPL);
CP2_FUNC(MVMVA);
CP2_FUNCNC(NCDS);
CP2_FUNCNC(NCDT);
CP2_FUNCNC(CDP);
CP2_FUNCNC(NCCS);
CP2_FUNCNC(CC);
CP2_FUNCNC(NCS);
CP2_FUNCNC(NCT);
CP2_FUNC(SQR);
CP2_FUNC(DCPL);
CP2_FUNCNC(DPCT);
CP2_FUNCNC(AVSZ3);
CP2_FUNCNC(AVSZ4);
CP2_FUNCNC(RTPT);
CP2_FUNC(GPF);
CP2_FUNC(GPL);
CP2_FUNCNC(NCCT);

static void recHLE() {

    //CALLFunc((u32) psxHLEt[psxRegs.code & 0xffff]);
    CALLFunc((u32) psxHLEt[psxRegs.code & 0x7f]);
    branch = 2;
    iRet();
}

static void (*recBSC[64])() = {
    recSPECIAL, recREGIMM, recJ, recJAL, recBEQ, recBNE, recBLEZ, recBGTZ,
    recADDI, recADDIU, recSLTI, recSLTIU, recANDI, recORI, recXORI, recLUI,
    recCOP0, recNULL, recCOP2, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recLB, recLH, recLWL, recLW, recLBU, recLHU, recLWR, recNULL,
    recSB, recSH, recSWL, recSW, recNULL, recNULL, recSWR, recNULL,
    recNULL, recNULL, recLWC2, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recSWC2, recHLE, recNULL, recNULL, recNULL, recNULL
};

static void (*recSPC[64])() = {
    recSLL, recNULL, recSRL, recSRA, recSLLV, recNULL, recSRLV, recSRAV,
    recJR, recJALR, recNULL, recNULL, recSYSCALL, recBREAK, recNULL, recNULL,
    recMFHI, recMTHI, recMFLO, recMTLO, recNULL, recNULL, recNULL, recNULL,
    recMULT, recMULTU, recDIV, recDIVU, recNULL, recNULL, recNULL, recNULL,
    recADD, recADDU, recSUB, recSUBU, recAND, recOR, recXOR, recNOR,
    recNULL, recNULL, recSLT, recSLTU, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recREG[32])() = {
    recBLTZ, recBGEZ, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recBLTZAL, recBGEZAL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recCP0[32])() = {
    recMFC0, recNULL, recCFC0, recNULL, recMTC0, recNULL, recCTC0, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recRFE, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

static void (*recCP2[64])() = {
    recBASIC, recRTPS, recNULL, recNULL, recNULL, recNULL, recNCLIP, recNULL, // 00
    recNULL, recNULL, recNULL, recNULL, recOP, recNULL, recNULL, recNULL, // 08
    recDPCS, recINTPL, recMVMVA, recNCDS, recCDP, recNULL, recNCDT, recNULL, // 10
    recNULL, recNULL, recNULL, recNCCS, recCC, recNULL, recNCS, recNULL, // 18
    recNCT, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, // 20
    recSQR, recDCPL, recDPCT, recNULL, recNULL, recAVSZ3, recAVSZ4, recNULL, // 28 
    recRTPT, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, // 30
    recNULL, recNULL, recNULL, recNULL, recNULL, recGPF, recGPL, recNCCT // 38
};

static void (*recCP2BSC[32])() = {
    recMFC2, recNULL, recCFC2, recNULL, recMTC2, recNULL, recCTC2, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL,
    recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL, recNULL
};

void recRecompile() {
    char *p;
    u32 *ptr;
    int i;
    
    // initialize state variables
    UniqueRegAlloc = 1;
    HWRegUseCount = 0;
    memset(HWRegisters, 0, sizeof (HWRegisters));
    for (i = 0; i < NUM_HW_REGISTERS; i++)
        HWRegisters[i].code = cpuHWRegisters[NUM_HW_REGISTERS - i - 1];

    // reserve the special psxReg register
    HWRegisters[0].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
    HWRegisters[0].private = PSXREGS;
    HWRegisters[0].k = (u32) & psxRegs;

    HWRegisters[1].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
    HWRegisters[1].private = PSXMEM;
    HWRegisters[1].k = (u32) & psxM;

    // reserve the special psxRegs.cycle register
    //HWRegisters[1].usage = HWUSAGE_SPECIAL | HWUSAGE_RESERVED | HWUSAGE_HARDWIRED;
    //HWRegisters[1].private = CYCLECOUNT;

    //memset(iRegs, 0, sizeof(iRegs));
    for (i = 0; i < NUM_REGISTERS; i++) {
        iRegs[i].state = ST_UNK;
        iRegs[i].reg = -1;
    }
    iRegs[0].k = 0;
    iRegs[0].state = ST_CONST;

    /* if ppcPtr reached the mem limit reset whole mem */
    if (((u32) ppcPtr - (u32) recMem) >= (RECMEM_SIZE - 0x10000)) // fix me. don't just assume 0x10000
        recReset();
#ifdef TAG_CODE
    ppcAlign();
#else
    ppcAlign(4);
#endif
    ptr = ppcPtr;

    // tell the LUT where to find us
    PC_REC32(psxRegs.pc) = (u32) ppcPtr;

    pcold = pc = psxRegs.pc;

    //where did 500 come from?
    for (count = 0; count < 500;) {
        p = (char *) PSXM(pc);
        if (p == NULL) recError();
        psxRegs.code = SWAP32(*(u32 *) p);
        pc += 4;
        count++;
        recBSC[psxRegs.code >> 26]();

        if (branch) {
            branch = 0;
            break;
        }
    }
    if (!branch) {
        iFlushRegs(pc);
        LIW(PutHWRegSpecial(PSXPC), pc);
        iRet();
    }

    invalidateCache((u32)(u8*)ptr, (u32)(u8*)ppcPtr);
	/*
	if (do_disasm || force_disasm) {
		u32* dp=ptr;
		while(dp<ppcPtr)
		{
			disassemble((u32)dp,*dp);
			++dp;
		}
		do_disasm=0;
	}
    */
#ifdef TAG_CODE
    sprintf((char *) ppcPtr, "PC=%08x", pcold); //causes misalignment
    ppcPtr += strlen((char *) ppcPtr);
#else
    sprintf((char *)ppcPtr, "PC=%08x", pcold);
    ppcPtr += strlen((char *)ppcPtr);
#endif
    dyna_used = ((u32) ppcPtr - (u32) recMem) / 1024;
}


R3000Acpu psxRec = {
    recInit,
    recReset,
    recExecute,
    recExecuteBlock,
    recClear,
    recShutdown
};
