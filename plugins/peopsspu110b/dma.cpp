/***************************************************************************
                            dma.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"

#define _IN_DMA

#include "externals.h"
#include "registers.h"



#include <stdio.h>
extern FILE *fp_spu_log;


//#define SPU_LOG

////////////////////////////////////////////////////////////////////////
// READ DMA (one value)
////////////////////////////////////////////////////////////////////////

unsigned short CALLBACK SPUreadDMA(void)
{
 unsigned short s;

 s=spuMem[spuAddr>>1];


 spuAddr+=2;
 if(spuAddr>0x7ffff) spuAddr=0;

 iSpuAsyncWait=0;

 return s;
}

////////////////////////////////////////////////////////////////////////
// READ DMA (many values)
////////////////////////////////////////////////////////////////////////

extern void (CALLBACK *irqCallback)(void);                  // func of main emu, called on spu irq
void CALLBACK SPUreadDMAMem(unsigned short * pusPSXMem,int iSize)
{
 int i;


#ifdef SPU_LOG
 if( !fp_spu_log ){
	 fp_spu_log = fopen( "spu-log.txt", "w" );
	}
 fprintf( fp_spu_log, "DMA-R %X = %X\n", spuAddr, iSize );
#endif


#if 0
 // illegal mode
 if( (spuCtrl & CTRL_DMA_F) != CTRL_DMA_R ) {
	 spuAddr = 0x1008;
 } else {
	 Check_IRQ( spuAddr, 1 );
 }
#endif


 spuStat |= STAT_DATA_BUSY;


 for(i=0;i<iSize;i++)
  {
	 Check_IRQ( spuAddr, 0 );

	 // guesswork
	 //if( (spuCtrl & CTRL_DMA_F) == CTRL_DMA_R ) {
	 {
		*pusPSXMem++=spuMem[spuAddr>>1];                    // spu addr got by writeregister
	 }
   spuAddr+=2;                                         // inc spu addr

	 
	 // Guesswork based on Vib Ribbon (dma-w)
	 // - creates a dma hang?
   if(spuAddr>0x7ffff) break;
 }

 iSpuAsyncWait=0;


 spuStat &= ~STAT_DATA_BUSY;
 spuStat &= ~STAT_DMA_NON;
 spuStat &= ~STAT_DMA_W;
 spuStat |= STAT_DMA_R;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

// to investigate: do sound data updates by writedma affect spu
// irqs? Will an irq be triggered, if new data is written to
// the memory irq address?

////////////////////////////////////////////////////////////////////////
// WRITE DMA (one value)
////////////////////////////////////////////////////////////////////////
  
void CALLBACK SPUwriteDMA(unsigned short val)
{
 spuMem[spuAddr>>1] = val;                             // spu addr got by writeregister

 spuAddr+=2;                                           // inc spu addr
 if(spuAddr>0x7ffff) spuAddr=0;                        // wrap

 iSpuAsyncWait=0;

}

////////////////////////////////////////////////////////////////////////
// WRITE DMA (many values)
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUwriteDMAMem(unsigned short * pusPSXMem,int iSize)
{
 int i;


#ifdef SPU_LOG
 if( !fp_spu_log ){
	 fp_spu_log = fopen( "spu-log.txt", "w" );
	}
 fprintf( fp_spu_log, "DMA-W %X = %X\n", spuAddr, iSize );
#endif



#if 0
 // illegal mode
 if( (spuCtrl & CTRL_DMA_F) != CTRL_DMA_W ) {
	 spuAddr = 0x1008;
 } else {
	 Check_IRQ( spuAddr, 1 );
 }
#endif


 spuStat |= STAT_DATA_BUSY;


 for(i=0;i<iSize;i++)
  {
	 Check_IRQ( spuAddr, 0 );

	 // guesswork
	 //if( (spuCtrl & CTRL_DMA_F) == CTRL_DMA_W ) {
	 {
		 spuMem[spuAddr>>1] = *pusPSXMem++;                  // spu addr got by writeregister
	 }
   spuAddr+=2;                                         // inc spu addr

	 
	 // Vib Ribbon - stop transfer (reverb playback)
   if(spuAddr>0x7ffff) break;
 }
 
 iSpuAsyncWait=0;


 spuStat &= ~STAT_DATA_BUSY;
 spuStat &= ~STAT_DMA_NON;
 spuStat &= ~STAT_DMA_R;
 spuStat |= STAT_DMA_W;
}

////////////////////////////////////////////////////////////////////////

