#ifndef LIBXENON_VM_H
#define	LIBXENON_VM_H

#ifdef	__cplusplus
extern "C"
{
#endif

typedef enum { MEM_LW,   MEM_LH,   MEM_LB,   MEM_LD,
               MEM_LWU,  MEM_LHU,  MEM_LBU,
               MEM_LWC1, MEM_LDC1, MEM_LWL,	 MEM_LWR,
               MEM_SW,   MEM_SH,   MEM_SB,   MEM_SD,
               MEM_SWC1, MEM_SDC1                    } memType;


extern int failsafeRec;
			   
#define FAILSAFE_REC_NO_LINK 1
#define FAILSAFE_REC_NO_VM 2
	
void recInitDynaMemVM();
void recDestroyDynaMemVM();
void recCallDynaMemVM(int rs_reg, int rt_reg, memType type, int immed);


#ifdef	__cplusplus
}
#endif

#endif	/* LIBXENON_VM_H */

