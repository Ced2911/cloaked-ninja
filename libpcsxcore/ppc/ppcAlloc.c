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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA 02111-1307 USA
 */

#ifdef _MSC_VER_
#pragma warning(disable:4244)
#pragma warning(disable:4761)
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "../psxcommon.h"
#include "ppc.h"
#include "reguse.h"
#include "../r3000a.h"
#include "../psxhle.h"
#include "../psxhw.h"
#include "ppcAlloc.h"

ppcMipsRec ppcRec;

HWRegister HWRegisters[NUM_HW_REGISTERS];
int HWRegUseCount;
static int DstCPUReg;
static int UniqueRegAlloc;

int GetFreeHWReg();
void InvalidateCPURegs();
void FlushHWReg(int index);
void MapPsxReg32(int reg);
void FlushPsxReg32(int hwreg);
int UpdateHWRegUsage(int hwreg, int usage);
int GetSpecialIndexFromHWRegs(int which);
int MapRegSpecial(int which);
void FlushRegSpecial(int hwreg);
void recRecompile();
void recError();


static int GetFreeHWReg()
{
	int i, least, index;

	if (DstCPUReg != -1) {
		index = GetHWRegFromCPUReg(DstCPUReg);
		DstCPUReg = -1;
	} else {
		// LRU algorith with a twist ;)
		for (i=0; i<NUM_HW_REGISTERS; i++) {
			if (!(HWRegisters[i].usage & HWUSAGE_RESERVED)) {
				break;
			}
		}

		least = HWRegisters[i].lastUsed; index = i;
		for (; i<NUM_HW_REGISTERS; i++) {
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

		// Cycle the registers
		if (HWRegisters[index].usage == HWUSAGE_NONE) {
			for (; i<NUM_HW_REGISTERS; i++) {
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

static void FlushHWReg(int index)
{
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
void DisposeHWReg(int index)
{
	if (index < 0) return;
	if (HWRegisters[index].usage == HWUSAGE_NONE) return;

	HWRegisters[index].usage &= ~(HWUSAGE_READ | HWUSAGE_WRITE);
	if (HWRegisters[index].usage == HWUSAGE_NONE) {
		SysPrintf("Error! not correctly disposing register (r%i)", HWRegisters[index].code);
	}

	FlushHWReg(index);
}

// operated on cpu registers
__inline static void FlushCPURegRange(int start, int end)
{
	int i;

	if (end <= 0) end = 31;
	if (start <= 0) start = 0;

	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
			if (HWRegisters[i].flush)
				FlushHWReg(i);
	}

	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code >= start && HWRegisters[i].code <= end)
			FlushHWReg(i);
	}
}

void FlushAllHWReg()
{
	FlushCPURegRange(0,31);
}

void InvalidateCPURegs()
{
	FlushCPURegRange(0,12);
}

static void MoveHWRegToCPUReg(int cpureg, int hwreg)
{
	int dstreg;

	if (HWRegisters[hwreg].code == cpureg)
		return;

	dstreg = GetHWRegFromCPUReg(cpureg);

	HWRegisters[dstreg].usage &= ~(HWUSAGE_HARDWIRED | HWUSAGE_ARG);
	if (HWRegisters[hwreg].usage & (HWUSAGE_READ | HWUSAGE_WRITE)) {
		FlushHWReg(dstreg);
		MR(HWRegisters[dstreg].code, HWRegisters[hwreg].code);
	} else {
		if (HWRegisters[dstreg].usage & (HWUSAGE_READ | HWUSAGE_WRITE)) {
			MR(HWRegisters[hwreg].code, HWRegisters[dstreg].code);
		}
		else if (HWRegisters[dstreg].usage != HWUSAGE_NONE) {
			FlushHWReg(dstreg);
		}
	}

	HWRegisters[dstreg].code = HWRegisters[hwreg].code;
	HWRegisters[hwreg].code = cpureg;
}

static int UpdateHWRegUsage(int hwreg, int usage)
{
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

int GetHWRegFromCPUReg(int cpureg)
{
	int i;
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].code == cpureg) {
			return i;
		}
	}

	SysPrintf("Error! Register location failure (r%i)", cpureg);
	return 0;
}

// this function operates on cpu registers
void SetDstCPUReg(int cpureg)
{
	DstCPUReg = cpureg;
}

static void MapPsxReg32(int reg)
{
	int hwreg = GetFreeHWReg();
	HWRegisters[hwreg].flush = FlushPsxReg32;
	HWRegisters[hwreg].reg = reg;

	if (iRegs[reg].reg != -1) {
		SysPrintf("error: double mapped psx register");
	}

	iRegs[reg].reg = hwreg;
	iRegs[reg].state |= ST_MAPPED;
}

static void FlushPsxReg32(int hwreg)
{
	int reg = HWRegisters[hwreg].reg;

	if (iRegs[reg].reg == -1) {
		SysPrintf("error: flushing unmapped psx register");
	}

	if (HWRegisters[hwreg].usage & HWUSAGE_WRITE) {
		if (ppcRec.branch) {
			/*int reguse = nextPsxRegUse(pc-8, reg);
			if (reguse == REGUSE_NONE || (reguse & REGUSE_READ))*/ {
				STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
		}
		} else {
			int reguse = nextPsxRegUse(ppcRec.pc-4, reg);
			if (reguse == REGUSE_NONE || (reguse & REGUSE_READ)) {
				STW(HWRegisters[hwreg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
			}
		}
	}

	iRegs[reg].reg = -1;
	iRegs[reg].state = ST_UNK;
}

int GetHWReg32(int reg)
{
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
		}
		else {
			LWZ(HWRegisters[iRegs[reg].reg].code, OFFSET(&psxRegs, &psxRegs.GPR.r[reg]), GetHWRegSpecial(PSXREGS));
		}
		HWRegisters[iRegs[reg].reg].usage &= ~HWUSAGE_RESERVED;
	}
	else if (DstCPUReg != -1) {
		int dst = DstCPUReg;
		DstCPUReg = -1;

		if (HWRegisters[iRegs[reg].reg].code < 13) {
			MoveHWRegToCPUReg(dst, iRegs[reg].reg);
		} else {
			MR(DstCPUReg, HWRegisters[iRegs[reg].reg].code);
		}
	}

	DstCPUReg = -1;

	return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

int PutHWReg32(int reg)
{
	int usage = HWUSAGE_PSXREG | HWUSAGE_WRITE;
	if (reg == 0) {
		return PutHWRegSpecial(REG_WZERO);
	}

	if (DstCPUReg != -1 && IsMapped(reg)) {
		if (HWRegisters[iRegs[reg].reg].code != DstCPUReg) {
			int tmp = DstCPUReg;
			DstCPUReg = -1;
			DisposeHWReg(iRegs[reg].reg);
			DstCPUReg = tmp;
		}
	}
	if (!IsMapped(reg)) {
		usage |= HWUSAGE_INITED;
		MapPsxReg32(reg);
	}

	DstCPUReg = -1;
	iRegs[reg].state &= ~ST_CONST;

	return UpdateHWRegUsage(iRegs[reg].reg, usage);
}

static int GetSpecialIndexFromHWRegs(int which)
{
	int i;
	for (i=0; i<NUM_HW_REGISTERS; i++) {
		if (HWRegisters[i].usage & HWUSAGE_SPECIAL) {
			if (HWRegisters[i].reg == which) {
				return i;
			}
		}
	}
	return -1;
}

static int MapRegSpecial(int which)
{
	int hwreg = GetFreeHWReg();
	HWRegisters[hwreg].flush = FlushRegSpecial;
	HWRegisters[hwreg].reg = which;

	return hwreg;
}

static void FlushRegSpecial(int hwreg)
{
	int which = HWRegisters[hwreg].reg;

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

int GetHWRegSpecial(int which)
{
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
			LIW(HWRegisters[index].code, (u32)&ppcRec.target);
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
	else if (DstCPUReg != -1) {
		int dst = DstCPUReg;
		DstCPUReg = -1;

		MoveHWRegToCPUReg(dst, index);
	}

	return UpdateHWRegUsage(index, usage);
}

int PutHWRegSpecial(int which)
{
	int index = GetSpecialIndexFromHWRegs(which);
	int usage = HWUSAGE_WRITE | HWUSAGE_SPECIAL;

	if (DstCPUReg != -1 && index != -1) {
		if (HWRegisters[index].code != DstCPUReg) {
			int tmp = DstCPUReg;
			DstCPUReg = -1;
			DisposeHWReg(index);
			DstCPUReg = tmp;
		}
	}
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
			switch (which) {
			case ARG1:
			case ARG2:
			case ARG3:
				SysPrintf("Don't use this... Call R%d\n", which);
				exit(0);
				break;
			}
		}
		HWRegisters[index].usage &= ~HWUSAGE_RESERVED;
		break;
	}

	DstCPUReg = -1;

	return UpdateHWRegUsage(index, usage);
}

void MapConst(int reg, u32 _const) {
	if (reg == 0)
		return;
	if (IsConst(reg) && iRegs[reg].k == _const)
		return;

	DisposeHWReg(iRegs[reg].reg);
	iRegs[reg].k = _const;
	iRegs[reg].state = ST_CONST;
}

void MapCopy(int dst, int src)
{
	// do it the lazy way for now
	MR(PutHWReg32(dst), GetHWReg32(src));
}

void iFlushReg(u32 nextpc, int reg) {
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

void iFlushRegs(u32 nextpc) {
	int i;

	for (i=1; i<NUM_REGISTERS; i++) {
		iFlushReg(nextpc, i);
	}
}
