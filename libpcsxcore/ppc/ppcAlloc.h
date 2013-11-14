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

#define NUM_REGISTERS	34

// Remember to invalidate the special registers if they are modified by compiler
enum {
    ARG1 = 3,
    ARG2 = 4,
    ARG3 = 5,
    PSXREGS,	// ptr
	PSXMEM,		// ptr
    CYCLECOUNT,	// ptr
    PSXPC,	// ptr
    TARGETPTR,	// ptr
    TARGET,	// ptr
    RETVAL,
    REG_RZERO,
    REG_WZERO
};

typedef struct {
    int code;
    u32 k;
    int usage;
    int lastUsed;
    
    void (*flush)(int hwreg);
    int reg;
} HWRegister;

typedef struct {
	int state;
	u32 k;
	int reg;
} iRegisters;

iRegisters iRegs[NUM_REGISTERS];

#define ST_UNK      0x00
#define ST_CONST    0x01
#define ST_MAPPED   0x02

#ifdef NO_CONSTANT
#define IsConst(reg) 0
#else
#define IsConst(reg)  (iRegs[reg].state & ST_CONST)
#endif
#define IsMapped(reg) (iRegs[reg].state & ST_MAPPED)

#define REG_LO			32
#define REG_HI			33

// Hardware register usage
#define HWUSAGE_NONE     0x00

#define HWUSAGE_READ     0x01
#define HWUSAGE_WRITE    0x02
#define HWUSAGE_CONST    0x04
#define HWUSAGE_ARG      0x08	/* used as an argument for a function call */

#define HWUSAGE_RESERVED 0x10	/* won't get flushed when flushing all regs */
#define HWUSAGE_SPECIAL  0x20	/* special purpose register */
#define HWUSAGE_HARDWIRED 0x40	/* specific hardware register mapping that is never disposed */
#define HWUSAGE_INITED    0x80
#define HWUSAGE_PSXREG    0x100




#undef _Op_
#define _Op_     _fOp_(psxRegs.code)
#undef _Funct_
#define _Funct_  _fFunct_(psxRegs.code)
#undef _Rd_
#define _Rd_     _fRd_(psxRegs.code)
#undef _Rt_
#define _Rt_     _fRt_(psxRegs.code)
#undef _Rs_
#define _Rs_     _fRs_(psxRegs.code)
#undef _Sa_
#define _Sa_     _fSa_(psxRegs.code)
#undef _Im_
#define _Im_     _fIm_(psxRegs.code)
#undef _Target_
#define _Target_ _fTarget_(psxRegs.code)

#undef _Imm_
#define _Imm_	 _fImm_(psxRegs.code)
#undef _ImmU_
#define _ImmU_	 _fImmU_(psxRegs.code)

#undef PC_REC
#undef PC_REC8
#undef PC_REC16
#undef PC_REC32
#define PC_REC(x)	(psxRecLUT[x >> 16] + (x & 0xffff))
#define PC_REC8(x)	(*(u8 *)PC_REC(x))
#define PC_REC16(x) (*(u16*)PC_REC(x))
#define PC_REC32(x) (*(u32*)PC_REC(x))

#define OFFSET(X,Y) ((u32)(Y)-(u32)(X))

#define RECMEM_SIZE		(12*1024*1024)

typedef struct ppcMipsRec {
	u32 pc;			/* recompiler pc */
	u32 pcold;		/* recompiler oldpc */
	int count;		/* recompiler intruction count */
	int branch;		/* set for branch */
	u32 target;		/* branch target */
	u32 resp;

} ppcMipsRec;

extern ppcMipsRec ppcRec;
extern HWRegister HWRegisters[NUM_HW_REGISTERS];
extern int HWRegUseCount;

void SetDstCPUReg(int cpureg);
void DisposeHWReg(int index);

int GetHWReg32(int reg);
int GetHWRegFromCPUReg(int cpureg);
int GetHWRegSpecial(int which);

int PutHWReg32(int reg);
int PutHWRegSpecial(int which);

void FlushAllHWReg();
void iFlushReg(u32 nextpc, int reg);
void iFlushRegs(u32 nextpc);

void InvalidateCPURegs();

void ReleaseArgs();
void ReserveArgs(int args);

void MapConst(int reg, u32 _const);
void MapCopy(int dst, int src);