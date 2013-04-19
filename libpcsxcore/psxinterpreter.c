/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

/*
 * PSX assembly interpreter.
 */

#include "psxcommon.h"
#include "r3000a.h"
#include "gte.h"
#include "psxhle.h"

static int branch = 0;
static int branch2 = 0;
static u32 branchPC;

// These macros are used to assemble the repassembler functions

#ifdef PSXCPU_LOG
#define debugI() PSXCPU_LOG("%s\n", disR3000AF(psxRegs.code, psxRegs.pc)); 
#else
#define debugI()
#endif

// Used until i fix the drc, help to get a noticiable speedup !!!
#define DRC_COMPATIBLE

// try to avoid branch jump as soon as possible
//#define AGGRESSIF // slower ...

// use table instead of swith for psxdelay
// #define TIMING_TABLE // no perf gain

__inline void execI();


// Subsets
void (*psxBSC[64])();
void (*psxSPC[64])();
void (*psxREG[32])();
void (*psxCP0[32])();
void (*psxCP2[64])();
void (*psxCP2BSC[32])();

#ifdef AGGRESSIF
static __inline void execOp(u32 op);
#endif

static void delayRead(int reg, u32 bpc) {
	u32 rold, rnew;

//	SysPrintf("delayRead at %x!\n", psxRegs.pc);

	rold = psxRegs.GPR.r[reg];
#ifdef AGGRESSIF
	execOp(_Op_);
#else
	psxBSC[_Op_](); // branch delay load
#endif
	rnew = psxRegs.GPR.r[reg];

	psxRegs.pc = bpc;

	psxBranchTest();

	psxRegs.GPR.r[reg] = rold;
	execI(); // first branch opcode
	psxRegs.GPR.r[reg] = rnew;

	branch = 0;
}

static void delayWrite(int reg, u32 bpc) {

/*	SysPrintf("delayWrite at %x!\n", psxRegs.pc);

	SysPrintf("%s\n", disR3000AF(psxRegs.code, psxRegs.pc-4));
	SysPrintf("%s\n", disR3000AF(PSXMu32(bpc), bpc));*/

	// no changes from normal behavior
#ifdef AGGRESSIF
	execOp(_Op_);
#else
	psxBSC[_Op_](); // branch delay load
#endif
	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

static void delayReadWrite(int reg, u32 bpc) {

//	SysPrintf("delayReadWrite at %x!\n", psxRegs.pc);

	// the branch delay load is skipped

	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}

// this defines shall be used with the tmp 
// of the next func (instead of _Funct_...)
#define _tFunct_  (_fFunct_(tmp))		// The funct part of the instruction register 
#define _tRd_     (_fRd_(tmp))			// The rd part of the instruction register 
#define _tRt_     (_fRt_(tmp))			// The rt part of the instruction register 
#define _tRs_     (_fRs_(tmp))			// The rs part of the instruction register 
#define _tSa_     (_fSa_(tmp))			// The sa part of the instruction register

#ifdef TIMING_TABLE

int ret_reg(int reg, u32 tmp) {
	switch (_tRt_) {
		case 0x00: case 0x01:
		case 0x10: case 0x11: // BLTZ/BGEZ...
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg) return 2;
			break;
	}
	return 0;
}

int ret_0(int reg, u32 tmp) {
	return 0;
}
int ret_jal(int reg, u32 tmp) {
	if (31 == reg) 
		return 3;
	return 0;
}

int ret_s_ops(int reg, u32 tmp) {
	if (_tRt_ == reg && _tRs_ == reg) return 1; else
	if (_tRs_ == reg) return 2; else
	if (_tRt_ == reg) return 3;
	return 0;
}

int ret__tRt__reg3(int reg, u32 tmp) {
	if (_tRt_ == reg) return 3;
	return 0;
}

int ret__tRt__reg2(int reg, u32 tmp) {
	if (_tRt_ == reg) return 2;
	return 0;
}

int ret_cop0(int reg, u32 tmp) {
	switch (_tFunct_) {
		case 0x00: // MFC0
			if (_tRt_ == reg) return 3;
			break;
		case 0x02: // CFC0
			if (_tRt_ == reg) return 3;
			break;
		case 0x04: // MTC0
			if (_tRt_ == reg) return 2;
			break;
		case 0x06: // CTC0
			if (_tRt_ == reg) return 2;
			break;
		// RFE just a break;
	}
	return 0;
}
int ret_cop2(int reg, u32 tmp) {
	switch (_tFunct_) {
		case 0x00: 
			switch (_tRs_) {
				case 0x00: // MFC2
					if (_tRt_ == reg) return 3;
					break;
				case 0x02: // CFC2
					if (_tRt_ == reg) return 3;
					break;
				case 0x04: // MTC2
					if (_tRt_ == reg) return 2;
					break;
				case 0x06: // CTC2
					if (_tRt_ == reg) return 2;
					break;
			}
			break;
		// RTPS... break;
	}
	return 0;
}
int ret_timing_lw(int reg, u32 tmp) {
	if (_tRt_ == reg) return 3; else
	if (_tRs_ == reg) return 2;
	return 0;
}
int ret_timing_l(int reg, u32 tmp) {
	if (_tRt_ == reg && _tRs_ == reg) return 1; else
	if (_tRs_ == reg) return 2; else
	if (_tRt_ == reg) return 3;
	return 0;
}
int ret_timing_s(int reg, u32 tmp) {
	if (_tRt_ == reg || _tRs_ == reg) return 2;
	return 0;
}
int ret_timing_c2(int reg, u32 tmp) {
	if (_tRs_ == reg) return 2;
	return 0;
}
int (*_timingSPC[64])(int reg, u32 tmp) = {
//	0          1          2        3        4        5        6        7
	ret_0	, ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x00
	ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x08
	ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x10
	ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x18
	ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x20
	ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x28
	ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x30
	ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , ret_0   , //	0x38
};
int timingSPC(int reg, u32 tmp) {
	//return _timingSPC[v](v, reg, tmp);
	switch (_tFunct_) {
		case 0x00: // SLL
		case 0x02: case 0x03: // SRL/SRA
			if (_tRd_ == reg && _tRt_ == reg) return 1; else
			if (_tRt_ == reg) return 2; else
			if (_tRd_ == reg) return 3;
			break;

		case 0x08: // JR
			if (_tRs_ == reg) return 2;
			break;
		case 0x09: // JALR
			if (_tRd_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRd_ == reg) return 3;
			break;

		// SYSCALL/BREAK just a break;

		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x24: case 0x25: case 0x26: case 0x27: 
		case 0x2a: case 0x2b: // ADD/ADDU...
		case 0x04: case 0x06: case 0x07: // SLLV...
			if (_tRd_ == reg && (_tRt_ == reg || _tRs_ == reg)) return 1; else
			if (_tRt_ == reg || _tRs_ == reg) return 2; else
			if (_tRd_ == reg) return 3;
			break;

		case 0x10: case 0x12: // MFHI/MFLO
			if (_tRd_ == reg) return 3;
			break;
		case 0x11: case 0x13: // MTHI/MTLO
			if (_tRs_ == reg) return 2;
			break;

		case 0x18: case 0x19:
		case 0x1a: case 0x1b: // MULT/DIV...
			if (_tRt_ == reg || _tRs_ == reg) return 2;
			break;
	}
	return 0;
}
int (*timingBSC[64])(int reg, u32 tmp) = {
//	0				1				2				3				4				5				6				7
	timingSPC ,		ret_reg,		ret_0,			ret_jal,		ret_0,			ret_0,			ret_0,			ret_0, //	0x00
	ret_s_ops ,		ret_s_ops,		ret_s_ops,		ret_s_ops,		ret_s_ops,		ret_s_ops,		ret_s_ops,		ret_s_ops, //	0x08
	ret_cop0  ,		ret_0,			ret_cop2,		ret_0,			ret_0,			ret_0,			ret_0,			ret_0, //	0x10
	ret_0,			ret_0,			ret_0,			ret_0,			ret_0,			ret_0,			ret_0,			ret_0, //	0x18
	ret_timing_l,	ret_timing_l,	ret_timing_lw,	ret_timing_l,	ret_timing_l,	ret_timing_l ,	ret_timing_lw,	ret_0, //	0x20
	ret_timing_s,	ret_timing_s,	ret_timing_s,	ret_timing_s,	ret_0,			ret_0,			ret_timing_s,	ret_0, //	0x28
	ret_0,			ret_0,			ret_timing_c2,	ret_0,			ret_0,			ret_0,			ret_0,			ret_0, //	0x30
	ret_0,			ret_0,			ret_timing_c2,	ret_0,			ret_0,			ret_0,			ret_0,			ret_0  //	0x38
};

int psxTestLoadDelay(int reg, u32 tmp) {
	if (tmp == 0) return 0; // NOP
	
	return timingBSC[_fOp_(tmp)](reg, tmp);

	return 0;
}


#else
int psxTestLoadDelay(int reg, u32 tmp) {
	if (tmp == 0) return 0; // NOP
	switch (_fOp_(tmp)) {
		case 0x00: // SPECIAL
			switch (_tFunct_) {
				case 0x00: // SLL
				case 0x02: case 0x03: // SRL/SRA
					if (_tRd_ == reg && _tRt_ == reg) return 1; else
					if (_tRt_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x08: // JR
					if (_tRs_ == reg) return 2;
					break;
				case 0x09: // JALR
					if (_tRd_ == reg && _tRs_ == reg) return 1; else
					if (_tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				// SYSCALL/BREAK just a break;

				case 0x20: case 0x21: case 0x22: case 0x23:
				case 0x24: case 0x25: case 0x26: case 0x27: 
				case 0x2a: case 0x2b: // ADD/ADDU...
				case 0x04: case 0x06: case 0x07: // SLLV...
					if (_tRd_ == reg && (_tRt_ == reg || _tRs_ == reg)) return 1; else
					if (_tRt_ == reg || _tRs_ == reg) return 2; else
					if (_tRd_ == reg) return 3;
					break;

				case 0x10: case 0x12: // MFHI/MFLO
					if (_tRd_ == reg) return 3;
					break;
				case 0x11: case 0x13: // MTHI/MTLO
					if (_tRs_ == reg) return 2;
					break;

				case 0x18: case 0x19:
				case 0x1a: case 0x1b: // MULT/DIV...
					if (_tRt_ == reg || _tRs_ == reg) return 2;
					break;
			}
			break;

		case 0x01: // REGIMM
			switch (_tRt_) {
				case 0x00: case 0x01:
				case 0x10: case 0x11: // BLTZ/BGEZ...
					// Xenogears - lbu v0 / beq v0
					// - no load delay (fixes battle loading)
					break;

					if (_tRs_ == reg) return 2;
					break;
			}
			break;

		// J would be just a break;
		case 0x03: // JAL
			if (31 == reg) return 3;
			break;

		case 0x04: case 0x05: // BEQ/BNE
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg || _tRt_ == reg) return 2;
			break;

		case 0x06: case 0x07: // BLEZ/BGTZ
			// Xenogears - lbu v0 / beq v0
			// - no load delay (fixes battle loading)
			break;

			if (_tRs_ == reg) return 2;
			break;

		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: // ADDI/ADDIU...
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x0f: // LUI
			if (_tRt_ == reg) return 3;
			break;

		case 0x10: // COP0
			switch (_tFunct_) {
				case 0x00: // MFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x02: // CFC0
					if (_tRt_ == reg) return 3;
					break;
				case 0x04: // MTC0
					if (_tRt_ == reg) return 2;
					break;
				case 0x06: // CTC0
					if (_tRt_ == reg) return 2;
					break;
				// RFE just a break;
			}
			break;

		case 0x12: // COP2
			switch (_tFunct_) {
				case 0x00: 
					switch (_tRs_) {
						case 0x00: // MFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x02: // CFC2
							if (_tRt_ == reg) return 3;
							break;
						case 0x04: // MTC2
							if (_tRt_ == reg) return 2;
							break;
						case 0x06: // CTC2
							if (_tRt_ == reg) return 2;
							break;
					}
					break;
				// RTPS... break;
			}
			break;

		case 0x22: case 0x26: // LWL/LWR
			if (_tRt_ == reg) return 3; else
			if (_tRs_ == reg) return 2;
			break;

		case 0x20: case 0x21: case 0x23:
		case 0x24: case 0x25: // LB/LH/LW/LBU/LHU
			if (_tRt_ == reg && _tRs_ == reg) return 1; else
			if (_tRs_ == reg) return 2; else
			if (_tRt_ == reg) return 3;
			break;

		case 0x28: case 0x29: case 0x2a:
		case 0x2b: case 0x2e: // SB/SH/SWL/SW/SWR
			if (_tRt_ == reg || _tRs_ == reg) return 2;
			break;

		case 0x32: case 0x3a: // LWC2/SWC2
			if (_tRs_ == reg) return 2;
			break;
	}

	return 0;
}

#endif
void psxDelayTest(int reg, u32 bpc) {
	u32 *code;
	u32 tmp;

	// Don't execute yet - just peek
	code = (u32 *)PSXM(psxRegs.pc);
	tmp = SWAP32(*code);

	branch = 1;

	switch (psxTestLoadDelay(reg, tmp)) {
		case 1:
			delayReadWrite(reg, bpc); return;
		case 2:
			delayRead(reg, bpc); return;
		case 3:
			delayWrite(reg, bpc); return;
	}

#ifdef AGGRESSIF
	execOp(_Op_);
#else
	psxBSC[_Op_](); // branch delay load
#endif

	branch = 0;
	psxRegs.pc = bpc;

	psxBranchTest();
}


static u32 psxBranchNoDelay(void) {
    u32 *code;
    u32 temp;

    code = (u32 *)PSXM(psxRegs.pc);
    psxRegs.code = SWAP32(*code);

    switch (_Op_) {
        case 0x00: // SPECIAL
            switch (_Funct_) {
                case 0x08: // JR
                    return _u32(_rRs_);
                case 0x09: // JALR
                    temp = _u32(_rRs_);
                    if (_Rd_) { _SetLink(_Rd_); }
                    return temp;
            }
            break;
        case 0x01: // REGIMM
            switch (_Rt_) {
                case 0x00: // BLTZ
                    if (_i32(_rRs_) < 0)
                        return _BranchTarget_;
                    break;
                case 0x01: // BGEZ
                    if (_i32(_rRs_) >= 0)
                        return _BranchTarget_;
                    break;
                case 0x08: // BLTZAL
                    if (_i32(_rRs_) < 0) {
                        _SetLink(31);
                        return _BranchTarget_;
                    }
                    break;
                case 0x09: // BGEZAL
                    if (_i32(_rRs_) >= 0) {
                        _SetLink(31);
                        return _BranchTarget_;
                    }
                    break;
            }
            break;
        case 0x02: // J
            return _JumpTarget_;
        case 0x03: // JAL
            _SetLink(31);
            return _JumpTarget_;
        case 0x04: // BEQ
            if (_i32(_rRs_) == _i32(_rRt_))
                return _BranchTarget_;
            break;
        case 0x05: // BNE
            if (_i32(_rRs_) != _i32(_rRt_))
                return _BranchTarget_;
            break;
        case 0x06: // BLEZ
            if (_i32(_rRs_) <= 0)
                return _BranchTarget_;
            break;
        case 0x07: // BGTZ
            if (_i32(_rRs_) > 0)
                return _BranchTarget_;
            break;
    }

    return (u32)-1;
}

__inline void doBranch(u32 tar) {
	u32 *code;
	u32 tmp;

	u32 pc = psxRegs.pc + 4;
	u32 cycle = psxRegs.cycle + BIAS;

	branch2 = branch = 1;
	branchPC = tar;
	/*
	if (psxDelayBranchTest(tar))
        return;
	*/
	// branch delay slot
	code = (u32 *)PSXM(psxRegs.pc);
	psxRegs.code = SWAP32(*code);

	debugI();

	psxRegs.pc = pc;
	psxRegs.cycle = cycle;

	// check for load delay
	tmp = _Op_;
#if 0
	switch (tmp) {
		case 0x10: // COP0
			switch (_Rs_) {
				case 0x00: // MFC0
				case 0x02: // CFC0
					psxDelayTest(_Rt_, branchPC);
					return;
			}
			break;
		case 0x12: // COP2
			switch (_Funct_) {
				case 0x00:
					switch (_Rs_) {
						case 0x00: // MFC2
						case 0x02: // CFC2
							psxDelayTest(_Rt_, branchPC);
							return;
					}
					break;
			}
			break;
		case 0x32: // LWC2
			psxDelayTest(_Rt_, branchPC);
			return;
		default:
			if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
				psxDelayTest(_Rt_, branchPC);
				return;
			}
			break;
	}
#else 	
	if (tmp >= 0x20 && tmp <= 0x26) { // LB/LH/LWL/LW/LBU/LHU/LWR
		psxDelayTest(_Rt_, branchPC);
		return;
	}

	// tmp 
	switch (tmp) {
		case 0x10: // COP0
			switch (_Rs_) {
				case 0x00: // MFC0
				case 0x02: // CFC0
					psxDelayTest(_Rt_, branchPC);
					return;
			}
			break;
		case 0x12: // COP2
			switch (_Funct_) {
				case 0x00:
					switch (_Rs_) {
						case 0x00: // MFC2
						case 0x02: // CFC2
							psxDelayTest(_Rt_, branchPC);
							return;
					}
					break;
			}
			break;
		case 0x32: // LWC2
			psxDelayTest(_Rt_, branchPC);
			return;
		default:
			break;
	}
#endif

#ifdef AGGRESSIF
	execOp(_Op_);
#else
	psxBSC[_Op_](); // branch delay load
#endif

	branch = 0;
	psxRegs.pc = branchPC;

	psxBranchTest();
}

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

#if 0
void psxADDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }		// Rt = Rs + Im 	(Exception on Integer Overflow)
void psxADDIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) + _Imm_ ; }		// Rt = Rs + Im
void psxANDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) & _ImmU_; }		// Rt = Rs And Im
void psxORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) | _ImmU_; }		// Rt = Rs Or  Im
void psxXORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) ^ _ImmU_; }		// Rt = Rs Xor Im
void psxSLTI() 	{ if (!_Rt_) return; _rRt_ = _i32(_rRs_) < _Imm_ ; }		// Rt = Rs < Im		(Signed)
void psxSLTIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) < ((u32)_Imm_); }		// Rt = Rs < Im		(Unsigned)
#else

__declspec( naked ) int asmADDI( int x, int y ) {
	__asm 
	{
		// x is in r3
		// y is in r4
		add r3,r3,r4
		// The return value is in r3
		blr // Don’t forget the explicit ‘blr’
	}
}

// Rt = Rs + Im 	(Exception on Integer Overflow)
__inline void psxADDI() 	{ 
	if (!_Rt_) 
		return; 
	_rRt_ = _u32(_rRs_) + _Imm_ ; 
	//_rRt_ = asmADDI(_u32(_rRs_),_Imm_);
}

// Rt = Rs + Im
__inline void psxADDIU() { 
	if (!_Rt_) 
		return; 
	_rRt_ = _u32(_rRs_) + _Imm_ ; 
	// _rRt_ = asmADDI(_u32(_rRs_),_Imm_);
}

__inline void psxANDI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) & _ImmU_; }		// Rt = Rs And Im
__inline void psxORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) | _ImmU_; }		// Rt = Rs Or  Im
__inline void psxXORI() 	{ if (!_Rt_) return; _rRt_ = _u32(_rRs_) ^ _ImmU_; }		// Rt = Rs Xor Im
__inline void psxSLTI() 	{ if (!_Rt_) return; _rRt_ = _i32(_rRs_) < _Imm_ ; }		// Rt = Rs < Im		(Signed)
__inline void psxSLTIU() { if (!_Rt_) return; _rRt_ = _u32(_rRs_) < ((u32)_Imm_); }		// Rt = Rs < Im		(Unsigned)
#endif
/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
__inline void psxADD()	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt		(Exception on Integer Overflow)
__inline void psxADDU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) + _u32(_rRt_); }	// Rd = Rs + Rt
__inline void psxSUB() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt		(Exception on Integer Overflow)
__inline void psxSUBU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) - _u32(_rRt_); }	// Rd = Rs - Rt
__inline void psxAND() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) & _u32(_rRt_); }	// Rd = Rs And Rt
__inline void psxOR() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) | _u32(_rRt_); }	// Rd = Rs Or  Rt
__inline void psxXOR() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) ^ _u32(_rRt_); }	// Rd = Rs Xor Rt
__inline void psxNOR() 	{ if (!_Rd_) return; _rRd_ =~(_u32(_rRs_) | _u32(_rRt_)); }// Rd = Rs Nor Rt
__inline void psxSLT() 	{ if (!_Rd_) return; _rRd_ = _i32(_rRs_) < _i32(_rRt_); }	// Rd = Rs < Rt		(Signed)
__inline void psxSLTU() 	{ if (!_Rd_) return; _rRd_ = _u32(_rRs_) < _u32(_rRt_); }	// Rd = Rs < Rt		(Unsigned)

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
__inline void psxDIV() {
	if (_i32(_rRt_) != 0) {
		_i32(_rLo_) = _i32(_rRs_) / _i32(_rRt_);
		_i32(_rHi_) = _i32(_rRs_) % _i32(_rRt_);
	}
	else {
		_i32(_rLo_) = 0xffffffff;
		_i32(_rHi_) = _i32(_rRs_);
	}
}

__inline void psxDIVU() {
	if (_rRt_ != 0) {
		_rLo_ = _rRs_ / _rRt_;
		_rHi_ = _rRs_ % _rRt_;
	}
	else {
		_rLo_ = 0xffffffff;
		_rHi_ = _rRs_;
	}
}

__inline void psxMULT() {
	u64 res = (s64)((s64)_i32(_rRs_) * (s64)_i32(_rRt_));
	//u64 res = __mulh(_i32(_rRs_), _i32(_rRt_));

	psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
	psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

__inline void psxMULTU() {
	u64 res = (u64)((u64)_u32(_rRs_) * (u64)_u32(_rRt_));
	//u64 res = __umulh(_u32(_rRs_), _u32(_rRt_));

	psxRegs.GPR.n.lo = (u32)(res & 0xffffffff);
	psxRegs.GPR.n.hi = (u32)((res >> 32) & 0xffffffff);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32(op)      if(_i32(_rRs_) op 0) doBranch(_BranchTarget_);
#define RepZBranchLinki32(op)  if(_i32(_rRs_) op 0) { _SetLink(31); doBranch(_BranchTarget_); }

__inline void psxBGEZ()   { RepZBranchi32(>=) }      // Branch if Rs >= 0
__inline void psxBGEZAL() { RepZBranchLinki32(>=) }  // Branch if Rs >= 0 and link
__inline void psxBGTZ()   { RepZBranchi32(>) }       // Branch if Rs >  0
__inline void psxBLEZ()   { RepZBranchi32(<=) }      // Branch if Rs <= 0
__inline void psxBLTZ()   { RepZBranchi32(<) }       // Branch if Rs <  0
__inline void psxBLTZAL() { RepZBranchLinki32(<) }   // Branch if Rs <  0 and link

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
__inline void psxSLL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) << _Sa_; } // Rd = Rt << sa
__inline void psxSRA() { if (!_Rd_) return; _i32(_rRd_) = _i32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (arithmetic)
__inline void psxSRL() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) >> _Sa_; } // Rd = Rt >> sa (logical)

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/
__inline void psxSLLV() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) << _u32(_rRs_); } // Rd = Rt << rs
__inline void psxSRAV() { if (!_Rd_) return; _i32(_rRd_) = _i32(_rRt_) >> _u32(_rRs_); } // Rd = Rt >> rs (arithmetic)
__inline void psxSRLV() { if (!_Rd_) return; _u32(_rRd_) = _u32(_rRt_) >> _u32(_rRs_); } // Rd = Rt >> rs (logical)

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
__inline void psxLUI() { if (!_Rt_) return; _u32(_rRt_) = psxRegs.code << 16; } // Upper halfword of Rt = Im

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
__inline void psxMFHI() { if (!_Rd_) return; _rRd_ = _rHi_; } // Rd = Hi
__inline void psxMFLO() { if (!_Rd_) return; _rRd_ = _rLo_; } // Rd = Lo

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
__inline void psxMTHI() { _rHi_ = _rRs_; } // Hi = Rs
__inline void psxMTLO() { _rLo_ = _rRs_; } // Lo = Rs

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
__inline void psxBREAK() {
	// Break exception - psx rom doens't handles this
}

__inline void psxSYSCALL() {
	psxRegs.pc -= 4;
	psxException(0x20, branch);
}

__inline void psxRFE() {
//	SysPrintf("psxRFE\n");
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status & 0xfffffff0) |
						  ((psxRegs.CP0.n.Status & 0x3c) >> 2);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op)      if(_i32(_rRs_) op _i32(_rRt_)) doBranch(_BranchTarget_);

__inline void psxBEQ() {	RepBranchi32(==) }  // Branch if Rs == Rt
__inline void psxBNE() {	RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
__inline void psxJ()   {               doBranch(_JumpTarget_); }
__inline void psxJAL() {	_SetLink(31); doBranch(_JumpTarget_); }

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
__inline void psxJR()   {
	doBranch(_u32(_rRs_));
	psxJumpTest();
}

__inline void psxJALR() {
	u32 temp = _u32(_rRs_);
	if (_Rd_) { _SetLink(_Rd_); }
	doBranch(temp);
}

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define _oB_ (_u32(_rRs_) + _Imm_)

__inline void psxLB() {
	u32 pc = psxRegs.pc;
#ifndef DRC_COMPATIBLE
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}

#endif

	if (_Rt_) {
		_i32(_rRt_) = (signed char)psxMemRead8(_oB_); 
	} else {
		psxMemRead8(_oB_); 
	}
}

__inline void psxLBU() {
	u32 pc = psxRegs.pc;
#ifndef DRC_COMPATIBLE
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}

#endif

	if (_Rt_) {
		_u32(_rRt_) = psxMemRead8(_oB_);
	} else {
		psxMemRead8(_oB_); 
	}
}

__inline void psxLH() {
	u32 pc = psxRegs.pc;

#ifndef DRC_COMPATIBLE

	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif


	if (_Rt_) {
		_i32(_rRt_) = (short)psxMemRead16(_oB_);
	} else {
		psxMemRead16(_oB_);
	}
}

__inline void psxLHU() {
	u32 pc = psxRegs.pc;

#ifndef DRC_COMPATIBLE
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif


	if (_Rt_) {
		_u32(_rRt_) = psxMemRead16(_oB_);
	} else {
		psxMemRead16(_oB_);
	}
}

__inline void psxLW() {
	u32 pc = psxRegs.pc;

#ifndef DRC_COMPATIBLE
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}

#endif

	if (_Rt_) {
		_u32(_rRt_) = psxMemRead32(_oB_);
	} else {
		psxMemRead32(_oB_);
	}
}

u32 LWL_MASK[4] = { 0xffffff, 0xffff, 0xff, 0 };
u32 LWL_SHIFT[4] = { 24, 16, 8, 0 };

__inline void psxLWL() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);
	u32 pc = psxRegs.pc;

#ifndef DRC_COMPATIBLE
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif

	if (!_Rt_) return;
	_u32(_rRt_) =	( _u32(_rRt_) & LWL_MASK[shift]) | 
					( mem << LWL_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   4bcd   (mem << 24) | (reg & 0x00ffffff)
	1   34cd   (mem << 16) | (reg & 0x0000ffff)
	2   234d   (mem <<  8) | (reg & 0x000000ff)
	3   1234   (mem      ) | (reg & 0x00000000)
	*/
}

u32 LWR_MASK[4] = { 0, 0xff000000, 0xffff0000, 0xffffff00 };
u32 LWR_SHIFT[4] = { 0, 8, 16, 24 };

__inline void psxLWR() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);
	u32 pc = psxRegs.pc;
		
#ifndef DRC_COMPATIBLE
	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif


	if (!_Rt_) return;
	_u32(_rRt_) =	( _u32(_rRt_) & LWR_MASK[shift]) | 
					( mem >> LWR_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)
	*/
}

__inline void psxSB() { psxMemWrite8 (_oB_, _u8 (_rRt_)); }
__inline void psxSH() { psxMemWrite16(_oB_, _u16(_rRt_)); }
__inline void psxSW() { psxMemWrite32(_oB_, _u32(_rRt_)); }

u32 SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0 };
u32 SWL_SHIFT[4] = { 24, 16, 8, 0 };

__inline void psxSWL() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 mem = psxMemRead32(addr & ~3);

	psxMemWrite32(addr & ~3,  (_u32(_rRt_) >> SWL_SHIFT[shift]) |
			     (  mem & SWL_MASK[shift]) );
	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)
	*/
}

u32 SWR_MASK[4] = { 0, 0xff, 0xffff, 0xffffff };
u32 SWR_SHIFT[4] = { 0, 8, 16, 24 };

__inline void psxSWR() {
	u32 addr = _oB_;
	u32 shift = addr & 3;
	u32 dest = addr & ~3;
	u32 mem = psxMemRead32(dest);

	psxMemWrite32(dest,  (_u32(_rRt_) << SWR_SHIFT[shift]) |
			     (  mem & SWR_MASK[shift]) );

	/*
	Mem = 1234.  Reg = abcd

	0   abcd   (reg      ) | (mem & 0x00000000)
	1   bcd4   (reg <<  8) | (mem & 0x000000ff)
	2   cd34   (reg << 16) | (mem & 0x0000ffff)
	3   d234   (reg << 24) | (mem & 0x00ffffff)
	*/
}

/*********************************************************
* Moves between GPR and COPx                             *
* Format:  OP rt, fs                                     *
*********************************************************/
__inline void psxMFC0()
{
#ifndef DRC_COMPATIBLE
	u32 pc = psxRegs.pc;

	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif

	if (!_Rt_) return;
	
	_i32(_rRt_) = (int)_rFs_;
}

__inline void psxCFC0()
{
#ifndef DRC_COMPATIBLE
	u32 pc = psxRegs.pc;

	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif

	if (!_Rt_) 
		return;
	
	_i32(_rRt_) = (int)_rFs_;
}

__inline void psxTestSWInts() {
	// the next code is untested, if u know please
	// tell me if it works ok or not (linuzappz)
	if (psxRegs.CP0.n.Cause & psxRegs.CP0.n.Status & 0x0300 &&
		psxRegs.CP0.n.Status & 0x1) {
		psxException(psxRegs.CP0.n.Cause, branch);
	}
}

__inline void MTC0(int reg, u32 val) {
//	SysPrintf("MTC0 %d: %x\n", reg, val);
	switch (reg) {
		case 12: // Status
			psxRegs.CP0.r[12] = val;
			psxTestSWInts();
			break;

		case 13: // Cause
			psxRegs.CP0.n.Cause = val & ~(0xfc00);
			psxTestSWInts();
			break;

		default:
			psxRegs.CP0.r[reg] = val;
			break;
	}
}

__inline void psxMTC0() { MTC0(_Rd_, _u32(_rRt_)); }
__inline void psxCTC0() { MTC0(_Rd_, _u32(_rRt_)); }



__inline void psxMFC2()
{
#ifndef DRC_COMPATIBLE
	u32 pc = psxRegs.pc;

	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif
	gteMFC2();
}


__inline void psxCFC2()
{
#ifndef DRC_COMPATIBLE
	u32 pc = psxRegs.pc;

	// load delay = 1 latency
	if( branch == 0 )
	{
		// simulate: beq r0,r0,lw+4 / lw / (delay slot)
		psxRegs.pc = pc - 4;
		doBranch( pc );

		return;
	}
#endif
	gteCFC2();
}


/*********************************************************
* Unknow instruction (would generate an exception)       *
* Format:  ?                                             *
*********************************************************/
__inline void psxNULL() { 
#ifdef PSXCPU_LOG
	PSXCPU_LOG("psx: Unimplemented op %x\n", psxRegs.code);
#endif
}

__inline void psxSPECIAL() {
	psxSPC[_Funct_]();
}

/*
//	0          1          2        3        4        5        6        7	
	psxBLTZ  , psxBGEZ  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	//	0x00
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	//	0x08
	psxBLTZAL, psxBGEZAL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	//	0x10
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL	//	0x18
*/
void psxREGIMM() {
	u32 rt = _Rt_;
#ifdef AGGRESSIF
	// 0x00 0x01 0x10 0x11
	if (rt == 0x00) {
		psxBLTZ();
	} else if (rt == 0x01) {
		psxBGEZ();
	} else if (rt == 0x10) {
		psxBLTZAL();
	} else if (rt == 0x11) {
		psxBGEZAL();
	}
#else
	psxREG[_Rt_](); // branch delay load
#endif
}

void psxCOP0() {	
#ifdef AGGRESSIF
	u32 rs = _Rs_;
	/*
	//	0          1          2        3        4        5        6        7	
	psxMFC0, psxNULL, psxCFC0, psxNULL, psxMTC0, psxNULL, psxCTC0, psxNULL,	 	//	0x00
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	 	//	0x08
	psxRFE , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	 	//	0x10
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL		//	0x18
	*/
	/*
	void psxMTC0() { MTC0(_Rd_, _u32(_rRt_)); }
	void psxCTC0() { MTC0(_Rd_, _u32(_rRt_)); }
	*/
	switch(rs) {
	case 0x00:
		psxMFC0();
		break;
	case 0x02:
		psxCFC0();
		break;
		/*
	case 0x04:
		psxMTC0();
		break;
	case 0x06:
		psxCTC0();
		break;
		*/
	case 0x04:
	case 0x06:
		MTC0(_Rd_, _u32(_rRt_));
		break;
	case 0x10:
		psxRFE();
		break;
	}
#else
	psxCP0[_Rs_]();
#endif
}

void psxCOP2() {
	if ((psxRegs.CP0.n.Status & 0x40000000) == 0 )
		return;
#ifdef AGGRESSIF
	{
		u32 func = _Funct_;
		if (func == 0) {
			u32 rs = _Rs_;
			// psxMFC2, psxNULL, psxCFC2, psxNULL, gteMTC2, psxNULL, gteCTC2, psxNULL,
			switch(rs) {
			case 0x00:
				psxMFC2();
				break;
			case 0x02:
				psxCFC2();
				break;
			case 0x04:
				gteMTC2();
				break;
			case 0x06:
				gteCTC2();
				break;
			}
		} else {
			// call gte func
			psxCP2[func]();
		}
	}
#else
	psxCP2[_Funct_]();
#endif
}

void psxBASIC() {
	psxCP2BSC[_Rs_]();
}

void psxHLE() {
//	psxHLEt[psxRegs.code & 0xffff]();
	psxHLEt[psxRegs.code & 0x07]();		// HDHOSHY experimental patch
}

// execi
void (*psxBSC[64])() = {
//	0          1          2        3        4        5        6        7
	psxSPECIAL, psxREGIMM, psxJ   , psxJAL  , psxBEQ , psxBNE , psxBLEZ, psxBGTZ, //	0x00
	psxADDI   , psxADDIU , psxSLTI, psxSLTIU, psxANDI, psxORI , psxXORI, psxLUI , //	0x08
	psxCOP0   , psxNULL  , psxCOP2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL, //	0x10
	psxNULL   , psxNULL  , psxNULL, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL, //	0x18
	psxLB     , psxLH    , psxLWL , psxLW   , psxLBU , psxLHU , psxLWR , psxNULL, //	0x20
	psxSB     , psxSH    , psxSWL , psxSW   , psxNULL, psxNULL, psxSWR , psxNULL, //	0x28
	psxNULL   , psxNULL  , gteLWC2, psxNULL , psxNULL, psxNULL, psxNULL, psxNULL, //	0x30
	psxNULL   , psxNULL  , gteSWC2, psxHLE  , psxNULL, psxNULL, psxNULL, psxNULL  //	0x38
};

// psxSPECIAL
void (*psxSPC[64])() = {
	psxSLL , psxNULL , psxSRL , psxSRA , psxSLLV   , psxNULL , psxSRLV, psxSRAV,
	psxJR  , psxJALR , psxNULL, psxNULL, psxSYSCALL, psxBREAK, psxNULL, psxNULL,
	psxMFHI, psxMTHI , psxMFLO, psxMTLO, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxMULT, psxMULTU, psxDIV , psxDIVU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxADD , psxADDU , psxSUB , psxSUBU, psxAND    , psxOR   , psxXOR , psxNOR ,
	psxNULL, psxNULL , psxSLT , psxSLTU, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL,
	psxNULL, psxNULL , psxNULL, psxNULL, psxNULL   , psxNULL , psxNULL, psxNULL
};

// psxREGIMM
void (*psxREG[32])() = {
//	0          1          2        3        4        5        6        7	
	psxBLTZ  , psxBGEZ  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	//	0x00
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	//	0x08
	psxBLTZAL, psxBGEZAL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	//	0x10
	psxNULL  , psxNULL  , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL	//	0x18
};

// psxCOP0
void (*psxCP0[32])() = {
//	0          1          2        3        4        5        6        7	
	psxMFC0, psxNULL, psxCFC0, psxNULL, psxMTC0, psxNULL, psxCTC0, psxNULL,	 	//	0x00
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	 	//	0x08
	psxRFE , psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,	 	//	0x10
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL		//	0x18
};

// psxCOP2
void (*psxCP2[64])() = {
	psxBASIC, gteRTPS , psxNULL , psxNULL, psxNULL, psxNULL , gteNCLIP, psxNULL, // 00
	psxNULL , psxNULL , psxNULL , psxNULL, gteOP  , psxNULL , psxNULL , psxNULL, // 08
	gteDPCS , gteINTPL, gteMVMVA, gteNCDS, gteCDP , psxNULL , gteNCDT , psxNULL, // 10
	psxNULL , psxNULL , psxNULL , gteNCCS, gteCC  , psxNULL , gteNCS  , psxNULL, // 18
	gteNCT  , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 20
	gteSQR  , gteDCPL , gteDPCT , psxNULL, psxNULL, gteAVSZ3, gteAVSZ4, psxNULL, // 28 
	gteRTPT , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 30
	psxNULL , psxNULL , psxNULL , psxNULL, psxNULL, gteGPF  , gteGPL  , gteNCCT  // 38
};

// psxBASIC
void (*psxCP2BSC[32])() = {
	psxMFC2, psxNULL, psxCFC2, psxNULL, gteMTC2, psxNULL, gteCTC2, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
	psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL
};

///////////////////////////////////////////

static int intInit() {
	return 0;
}

static void intReset() {
	psxRegs.ICache_valid = FALSE;
}

static void intExecute() {
	for (;;) 
	{
		execI();
	}
}

static void intExecuteBlock() {
	branch2 = 0;
	while (!branch2){
		execI();
	}
}

static void intClear(u32 Addr, u32 Size) {
}

static void intShutdown() {
}

#ifdef AGGRESSIF
static __inline void execOp(u32 op) {
	switch(op) {
		// psxSPECIAL
		case 0x00:
			psxSPECIAL();
			break;
		case 0x01:
			psxREGIMM();
			break;
		case 0x10:
			psxCOP0();
			break;
		case 0x12:
			psxCOP2();
			break;
		default:
			psxBSC[op]();
	}
}
#endif

// interpreter execution
static void __execI() { 
	u32 pc = psxRegs.pc + 4;
	u32 cycle = psxRegs.cycle + BIAS;

	u32 *code = (u32 *)PSXM(psxRegs.pc);
	psxRegs.code = SWAP32(*code);

	debugI();

	// if (Config.Debug) ProcessDebug();
	
	psxRegs.pc = pc;
	psxRegs.cycle = cycle;
		
#ifdef AGGRESSIF
	execOp(_Op_);
#else
	psxBSC[_Op_]();
#endif
}


enum {
	SPECIAL,REGIMMM,J,JAL,BEQ,BNE,BLEZ,BGTZ,
	ADDI,ADDIU,SLTI,SLTIU,ANDI,ORI,XORI,LUI,
	COP0,COP1,COP2,COP3,OP14,OP15,OP16,OP17,
	OP18,OP19,OP1A,OP1B,OP1C,OP1D,OP1E,OP1F,
	LB,LH,LWL,LW,LBU,LHU,LWR,OP27,
	SB,SH,SWL,SW,OP2A,OP2B,SWR,OP2F,
	LWC0,LWC1,LWC2,LWC3,OP34,OP35,OP36,OP37,
	SWC0,SWC1,SWC2,SWC3,OP3C,OP3D,OP3E,OP3F
};

enum {
	SLL,SP01,SRL,SRA,SLLV,SP05,SRLV,SRAV,
	JR,JALR,SP0A,SP0B,SYSCALL,BREAK,SP0E,SP0F,
	MFHI,MTHI,MFLO,MTLO,SP14,SP15,SP16,SP17,
	MULT,MULTU,DIV,DIVU,SP1C,SP1D,SP1E,SP1F,
	ADD,ADDU,SUB,SUBU,AND,OR,XOR,NOR,
	SP28,SP29,SLT,SLTU,SP2C,SP2D,SP2E,SP2F
};

enum {
	BLTZ,BGEZ,BC02,BC03,BC04,BC05,BC06,BC07,
	BC08,BC09,BC0A,BC0B,BC0C,BC0D,BC0E,BC0F,
	BLTZAL,BGEZAL
};

enum {
	MFC = 0,CFC = 2, MTC = 4, CTC = 6
};

// interpreter execution - is compiler generating a better code ? i'm not sure ...
void execI() { 
	u32 pc = psxRegs.pc + 4;
	u32 cycle = psxRegs.cycle + BIAS;

	u32 *code = (u32 *)PSXM(psxRegs.pc);
	psxRegs.code = SWAP32(*code);

	debugI();

	if (!code) {
		/** nop **/
		return;
	}

	// if (Config.Debug) ProcessDebug();
	
	psxRegs.pc = pc;
	psxRegs.cycle = cycle;

	switch(_Op_) {
	case SPECIAL:
		switch(_Funct_) {
		case SLL:	psxSLL(); break;
		case SRL:	psxSRL(); break;
		case SRA:	psxSRA(); break;
		case SLLV:	psxSLLV(); break;
		case SRLV:	psxSRLV(); break;
		case SRAV:	psxSRAV(); break; 
		case ADD:	psxADD(); break;
		case SUB:	psxSUB(); break;
		case ADDU:	psxADDU(); break;
		case SUBU:	psxSUBU(); break;
		case AND:	psxAND(); break;
		case OR	:	psxOR(); break;
		case XOR:	psxXOR(); break;
		case NOR:	psxNOR(); break;
		case SLT:	psxSLT(); break;
		case SLTU:	psxSLTU(); break;
		case DIV:	psxDIV();break;
		case DIVU:	psxDIVU(); break;
		case MULT:	psxMULT();	break;
		case MULTU:	psxMULTU();	break;

		case MFHI:	psxMFHI(); break;
		case MFLO:	psxMFLO(); break;
		case MTHI:	psxMTHI(); break;
		case MTLO:	psxMTLO(); break;
		case JALR:	psxJALR(); break;
		case JR	:	psxJR(); break;

		case BREAK:	psxBREAK(); break;
		case SYSCALL: psxSYSCALL(); break;
		default:	 break;
		}
		break;

	case REGIMMM:
		switch(_Rt_){
		case BLTZAL:	psxBLTZAL(); break;
		case BLTZ:		psxBLTZ(); break;
		case BGEZAL:	psxBGEZAL(); break;
		case BGEZ:		psxBGEZ(); break;
		default:
			break;
		}
		break;

	case JAL:	psxJAL(); break;
	case J:		psxJ(); break;
	case BNE:	psxBNE(); break;
	case BEQ:	psxBEQ(); break;
	case BLEZ:	psxBLEZ(); break;
	case BGTZ:	psxBGTZ(); break;

	case ADDI:	psxADDI(); break;
	case ADDIU:	psxADDIU(); break;
	case SLTI:	psxSLTI(); break;
	case SLTIU:	psxSLTIU(); break;
	case ANDI:	psxANDI(); break;
	case ORI:	psxORI(); break;
	case XORI:	psxXORI(); break;
	case LUI:	psxLUI(); break;

	case COP0:
		psxCOP0();
		break;
	case COP2:
		psxCOP2();
		break;

	case SB:	psxSB(); break;
	case SH:	psxSH(); break;
	case SW:	psxSW(); break;
	case SWL:	psxSWL(); break;
	case SWR:	psxSWR(); break;

	case LB:	psxLB(); break;
	case LBU:	psxLBU(); break;
	case LH:	psxLH(); break;
	case LHU:	psxLHU(); break;
	case LW:	psxLW(); break;
	case LWL:	psxLWL(); break;
	case LWR:	psxLWR(); break;

	case SWC2:	gteSWC2(); break;
	case LWC2:	gteLWC2(); break;

	default:
		break;
	}
}

R3000Acpu psxInt = {
	intInit,
	intReset,
	intExecute,
	intExecuteBlock,
	intClear,
	intShutdown
};
