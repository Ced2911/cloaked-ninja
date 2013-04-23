#include <ppc/vm.h>
#include <assert.h>

#include "libxenon_vm.h"
#include "ppc.h"
#include "pR3000A.h"
#include "../libpcsxcore/psxmem.h"

#define MEMORY_VM_BASE 0x40000000
#define MEMORY_VM_SIZE (512*1024*1024)

#define PPC_OPCODE_SHIFT 26

#define PPC_OPCODE_ADDI  14
#define PPC_OPCODE_ADDIS 15
#define PPC_OPCODE_B     18

#define PPC_NOP 0x60000000

#define REG_ADDR_HOST 7

#define CHECK_INVALID_CODE() \
	RLWINM(5,addr_emu,18,14,29); \
	LIS(6,(u32)psxMemWLUT>>16); \
	LWZX(4,6,5); \
	CMPLWI(4,0); \
	preWrite = ppcPtr; \
	BEQ(0);

#define SET_INVALID_CODE(standalone) \
	if (standalone) { RLWINM(5,addr_emu,18,14,29); } \
	LIS(6,(u32)psxRecLUT>>16); \
	LWZX(4,6,5); \
	RLWINM(6,addr_emu,0,16,29); \
	LI(5,0); \
	ADDI(6,6,4); \
	STWX(5,4,6);


int failsafeRec=0;

void recCallDynaMem(int addr, int data, int type)
{
	if(addr!=3)
		MR(3, addr);

	if (type<MEM_SW)
	{
		switch (type)
		{
			case MEM_LB:
				CALLFunc((u32) psxMemRead8);
				EXTSB(data, 3);
				break;
			case MEM_LBU:
				CALLFunc((u32) psxMemRead8);
				MR(data,3);
				break;
			case MEM_LH:
				CALLFunc((u32) psxMemRead16);
				EXTSH(data, 3);
				break;
			case MEM_LHU:
				CALLFunc((u32) psxMemRead16);
				MR(data,3);
				break;
			case MEM_LW:
				CALLFunc((u32) psxMemRead32);
				MR(data,3);
				break;
		}
	}
	else
	{
		switch (type)
		{
			case MEM_SB:
	            RLWINM(4, data, 0, 24, 31);
				CALLFunc((u32) psxMemWrite8);
				break;
			case MEM_SH:
	            RLWINM(4, data, 0, 16, 31);
				CALLFunc((u32) psxMemWrite16);
				break;
			case MEM_SW:
				MR(4, data);
				CALLFunc((u32) psxMemWrite32);
				break;
		}
	}
}

void recCallDynaMemVM(int rs_reg, int rt_reg, memType type, int immed)
{
	u32 * preWrite=NULL;
	u32 * preCall=NULL;
	u32 * old_ppcPtr=NULL;

	InvalidateCPURegs();

	int base = GetHWReg32( rs_reg );
	int data = -1;
	int addr_emu = 3;

	if (type<MEM_SW)
		data = PutHWReg32( rt_reg );
	else
		data = GetHWReg32( rt_reg );

	ADDI(addr_emu, base, immed);

	if(!(failsafeRec&FAILSAFE_REC_NO_VM))
	{
		RLWINM(REG_ADDR_HOST,addr_emu,0,3,31);
		ADDIS(REG_ADDR_HOST,REG_ADDR_HOST,MEMORY_VM_BASE>>16);
		NOP(); // don't remove me (see rewriteDynaMemVM)

		// Perform the actual load
		switch (type)
		{
			case MEM_LB:
			{
//				LIS(REG_ADDR_HOST,0x3040);

				LBZ(data, 0, REG_ADDR_HOST);
				EXTSB(data, data);
				break;
			}
			case MEM_LBU:
			{
//				LIS(REG_ADDR_HOST,0x3041);

				LBZ(data, 0, REG_ADDR_HOST);
				break;
			}
			case MEM_LH:
			{
//				LIS(REG_ADDR_HOST,0x3042);

				LHBRX(data, 0, REG_ADDR_HOST);
				EXTSH(data, data);
				break;
			}
			case MEM_LHU:
			{
//				LIS(REG_ADDR_HOST,0x3043);

				LHBRX(data, 0, REG_ADDR_HOST);
				break;
			}
			case MEM_LW:
			{
//				LIS(REG_ADDR_HOST,0x3044);

				LWBRX(data, 0, REG_ADDR_HOST);
				break;
			}
			case MEM_SB:
			{
//				LIS(REG_ADDR_HOST,0x3050);

				STB(data, 0, REG_ADDR_HOST);
				SET_INVALID_CODE(1);
				break;
			}
			case MEM_SH:
			{
//				LIS(REG_ADDR_HOST,0x3052);

				STHBRX(data, 0, REG_ADDR_HOST);
				SET_INVALID_CODE(1);
				break;
			}
			case MEM_SW:
			{
				assert(data<4 || data >6);
				CHECK_INVALID_CODE();				

//				LIS(REG_ADDR_HOST,0x3054);

				STWBRX(data, 0, REG_ADDR_HOST);
				SET_INVALID_CODE(0);
				break;
			}
			default:
				assert(0);
		}

		// Skip over else
		preCall = ppcPtr;
		B(0);
	}

	if (preWrite!=NULL)
	{
		old_ppcPtr=ppcPtr;
		ppcPtr=preWrite;
		BEQ(old_ppcPtr-preWrite-1);
		ppcPtr=old_ppcPtr;
	}

	recCallDynaMem(addr_emu, data, type);

	if(!(failsafeRec&FAILSAFE_REC_NO_VM))
	{
		old_ppcPtr=ppcPtr;
		ppcPtr=preCall;
		B(old_ppcPtr-preCall-1);
		ppcPtr=old_ppcPtr;
	}
}

static void * rewriteDynaMemVM(void* fault_addr, void* accessed_addr)
{
	u32 * old_ppcPtr=ppcPtr;
    u32 * fault_op=(u32 *)fault_addr;
	u32 * op;
	u32 aa=(u32)accessed_addr;
	int scratch_installed=0;

	// scratchpad accesses (scratchpad offset trick)

	// sratchpad is mapped at the top of the previous VM page (0x1f7f0000).
	// when scratchpad is accessed the first time, I add an offset to the mem
	// address, so that it falls into the mapped scratchpad.
	// if the op that accesses the scratchpad also accesses registers
	// (even with offset, registers are still mapped to fault),
	// offset is removed until next stratchpad access.

	if((aa & 0x1ffffc00) == 0x1f800000)
	{
		op=fault_op;

		while(*op!=PPC_NOP )
		{
			--op;
		}

		if((fault_op-op)<7)
		{
			// install scratchpad offset			

//			printf("scratch ins %08x %08x\n",aa,fault_op-op);

			ppcPtr=op;
			ADDI(REG_ADDR_HOST,REG_ADDR_HOST,SCRATCHPAD_OFFSET);

			scratch_installed=1;
		}
		else
		{
			// remove scratchpad offset

			op=fault_op;

			while((*op>>PPC_OPCODE_SHIFT)!=PPC_OPCODE_ADDI || (*op&&0xffff)!=SCRATCHPAD_OFFSET)
			{
				--op;
			}

			printf("scratch rem %08x %08x\n",aa,fault_op-op);

			ppcPtr=op;
			NOP();
		}

		memicbi(op,4);
	}

    // enabling slow access by adding a jump from the fault address to the slow mem access code

	op=fault_op;

	while((*op>>PPC_OPCODE_SHIFT)!=PPC_OPCODE_B || (*op&1)!=0)
	{
		++op;
	}

	// branch op
	++op;

	u32 * first_slow_op=op;

	if(!scratch_installed)
	{
		ppcPtr=fault_op;
		B(first_slow_op-fault_op-1);

		memicbi(fault_op,4);
	}

	ppcPtr=old_ppcPtr;

	return first_slow_op;
}

void * recDynaMemVMSegfaultHandler(int pir_,void * srr0,void * dar,int write)
{
    if((u32)srr0>=(u32)recMem && (u32)srr0<(u32)recMem+RECMEM_SIZE)
    {
//        printf("Rewrite %d %p %p %d\n",pir_,srr0,dar,write);
        return rewriteDynaMemVM(srr0, dar);
    }
    else
    {
        printf("VM GPF !!! %d %p %p %d\n",pir_,srr0,dar,write);

		// use the standard segfault handler
	    vm_set_user_mapping_segfault_handler(NULL);
        return NULL;
    }
}



void recInitDynaMemVM()
{
    vm_set_user_mapping_segfault_handler(recDynaMemVMSegfaultHandler);

    u32 base=MEMORY_VM_BASE;
    
    // map ram
    vm_create_user_mapping(base,((u32)&psxM[0])&0x7fffffff,2*1024*1024,VM_WIMG_CACHED);

    // map bios
    vm_create_user_mapping(base+0x1fc00000,((u32)&psxR[0])&0x7fffffff,512*1024,VM_WIMG_CACHED_READ_ONLY);

    // map scratchpad (special mapping, see rewriteDynaMemVM)
    vm_create_user_mapping(base+0x1f7f0000,((u32)&psxM[0x210000])&0x7fffffff,64*1024,VM_WIMG_CACHED);
}

void recDestroyDynaMemVM()
{
    vm_set_user_mapping_segfault_handler(NULL);
    vm_destroy_user_mapping(MEMORY_VM_BASE,MEMORY_VM_SIZE);
}