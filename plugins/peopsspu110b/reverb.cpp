/***************************************************************************
reverb.c  -  description
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
// 2003/01/19 - Pete
// - added Neill's reverb (see at the end of file)
//
// 2002/12/26 - Pete
// - adjusted reverb handling
//
// 2002/08/14 - Pete
// - added extra reverb
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"
#include "registers.h"


#define _IN_REVERB

// will be included from spu.c
#ifdef _IN_SPU

////////////////////////////////////////////////////////////////////////
// globals
////////////////////////////////////////////////////////////////////////

// REVERB info and timing vars...

int *          sRVBPlay      = 0;
int *          sRVBEnd       = 0;
int *          sRVBStart     = 0;
int            iReverbOff    = -1;                          // some delay factor for reverb
int            iReverbRepeat = 0;
int            iReverbNum    = 1;    


static int iRvbCnt=0;                                  // this func will be called with 44.1 khz
			

////////////////////////////////////////////////////////////////////////
// SET REVERB
////////////////////////////////////////////////////////////////////////

void SetREVERB(unsigned short val)
{
	switch(val)
  {
	case 0x0000: iReverbOff=-1;  break;                                         // off
	case 0x007D: iReverbOff=32;  iReverbNum=2; iReverbRepeat=128;  break;       // ok room
		
	case 0x0033: iReverbOff=32;  iReverbNum=2; iReverbRepeat=64;   break;       // studio small
	case 0x00B1: iReverbOff=48;  iReverbNum=2; iReverbRepeat=96;   break;       // ok studio medium
	case 0x00E3: iReverbOff=64;  iReverbNum=2; iReverbRepeat=128;  break;       // ok studio large ok
		
	case 0x01A5: iReverbOff=128; iReverbNum=4; iReverbRepeat=32;   break;       // ok hall
	case 0x033D: iReverbOff=256; iReverbNum=4; iReverbRepeat=64;   break;       // space echo
	case 0x0001: iReverbOff=184; iReverbNum=3; iReverbRepeat=128;  break;       // echo/delay
	case 0x0017: iReverbOff=128; iReverbNum=2; iReverbRepeat=128;  break;       // half echo
	default:     iReverbOff=32;  iReverbNum=1; iReverbRepeat=0;    break;
  }
}

////////////////////////////////////////////////////////////////////////
// START REVERB
////////////////////////////////////////////////////////////////////////

INLINE void StartREVERB(SPUCHAN * pChannel)
{
	// note: reverb -write- flag, not play flag
	//if(pChannel->bReverb && (spuCtrl & CTRL_REVERB))               // reverb possible?
	if(pChannel->bReverb)               // reverb possible?
  {
		if(iUseReverb==2) pChannel->bRVBActive=1;
		else {
			if(iUseReverb==1 && iReverbOff>0)                   // -> fake reverb used?
			{
				pChannel->bRVBActive=1;                           // -> activate it
				pChannel->iRVBOffset=iReverbOff*APU_run;
				pChannel->iRVBRepeat=iReverbRepeat*APU_run;
				pChannel->iRVBNum   =iReverbNum;
			}
		}
  }
	else pChannel->bRVBActive=0;                          // else -> no reverb
}

////////////////////////////////////////////////////////////////////////
// HELPER FOR NEILL'S REVERB: re-inits our reverb mixing buf
////////////////////////////////////////////////////////////////////////

INLINE void InitREVERB(void)
{
	if(iUseReverb==2)
  {memset(sRVBStart,0,NSSIZE*2*4);}
}

////////////////////////////////////////////////////////////////////////
// STORE REVERB
////////////////////////////////////////////////////////////////////////

INLINE void StoreREVERB_CD(int left, int right,int ns)
{
	if(iUseReverb==0) return;
	else {
		if(iUseReverb==2) // -------------------------------- // Neil's reverb
		{
			const int iRxl=left;
			const int iRxr=right;
			
			ns<<=1;
			
			// -> we mix all active reverb channels into an extra buffer
			*(sRVBStart+ns)   += CLAMP16( *(sRVBStart+ns+0) + ( iRxl ) );
			*(sRVBStart+ns+1) += CLAMP16( *(sRVBStart+ns+1) + ( iRxr ) );
		}
	}
}


INLINE void StoreREVERB(SPUCHAN * pChannel,int ns)
{
	if(iUseReverb==0) return;
	else
	{
		if(iUseReverb==2) // -------------------------------- // Neil's reverb
		{
			// Suikoden 2 - Tinto Mines (phase invert)
			int iRxl=(pChannel->sval * pChannel->iLeftVolume)/0x4000;
			int iRxr=(pChannel->sval * pChannel->iRightVolume)/0x4000;
			
			// Breath of Fire 3 - fix over-reverb
			iRxl = (iRxl * iVolVoices) / 10;
			iRxr = (iRxr * iVolVoices) / 10;
			
			ns<<=1;
			
			// -> we mix all active reverb channels into an extra buffer
			*(sRVBStart+ns)   = CLAMP16( *(sRVBStart+ns+0) + ( iRxl ) );
			*(sRVBStart+ns+1) = CLAMP16( *(sRVBStart+ns+1) + ( iRxr ) );
		}
		else // --------------------------------------------- // Pete's easy fake reverb
		{
			int * pN;int iRn,iRr=0;
			
			// we use the half channel volume (/0x8000) for the first reverb effects, quarter for next and so on
			
			int iRxl=( pChannel->sval * pChannel->iLeftVolume )/0x8000;
			int iRxr=( pChannel->sval * pChannel->iRightVolume )/0x8000;
			
			for(iRn=1;iRn<=pChannel->iRVBNum;iRn++,iRr+=pChannel->iRVBRepeat,iRxl/=2,iRxr/=2)
			{
				pN=sRVBPlay+((pChannel->iRVBOffset+iRr+ns)<<1);
				if(pN>=sRVBEnd) pN=sRVBStart+(pN-sRVBEnd);
				
				(*pN)+=iRxl;
				(*pN) = CLAMP16(*pN);
				pN++;
				
				(*pN)+=iRxr;
				(*pN) = CLAMP16(*pN);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////

INLINE int g_buffer(int iOff)                          // get_buffer content helper: takes care about wraps
{
	short * p=(short *)spuMem;
	iOff=(iOff*4)+rvb.CurrAddr;
	while(iOff>0x3FFFF)       iOff=rvb.StartAddr+(iOff-0x40000);
	while(iOff<rvb.StartAddr) iOff=0x3ffff-(rvb.StartAddr-iOff);
	
	return (int)*(p+iOff);
}

////////////////////////////////////////////////////////////////////////

INLINE void s_buffer(int iOff,int iVal)                // set_buffer content helper: takes care about wraps and clipping
{
	short * p=(short *)spuMem;
	
	
	iOff=(iOff*4)+rvb.CurrAddr;
	while(iOff>0x3FFFF) iOff=rvb.StartAddr+(iOff-0x40000);
	while(iOff<rvb.StartAddr) iOff=0x3ffff-(rvb.StartAddr-iOff);
	if(iVal<-32768L) iVal=-32768L;if(iVal>32767L) iVal=32767L;
	
	*(p+iOff)=(short)iVal;
}

////////////////////////////////////////////////////////////////////////

INLINE void s_buffer1(int iOff,int iVal)                // set_buffer (+1 sample) content helper: takes care about wraps and clipping
{
	short * p=(short *)spuMem;
	iOff=(iOff*4)+rvb.CurrAddr+1;
	while(iOff>0x3FFFF) iOff=rvb.StartAddr+(iOff-0x40000);
	while(iOff<rvb.StartAddr) iOff=0x3ffff-(rvb.StartAddr-iOff);
	if(iVal<-32768L) iVal=-32768L;if(iVal>32767L) iVal=32767L;
	
	*(p+iOff)=(short)iVal;
}

////////////////////////////////////////////////////////////////////////

#if 0
/ *
* PlayStation Reverberation Algorithm (C)Dr.Hell, 2005 * PlayStation Reverberation Algorithm (C) Dr.Hell, 2005
* ?????????????????1?????????????? * Strictly speaking, the timing of the process is shifted left one sampling time,
* ???2??????????????? */ * Each sample is run every 2 hours * /
mAPF1    = *((ushort *) 0x1F801DC0) * 4; mAPF1 = * ((ushort *) 0x1F801DC0) * 4;
mAPF2    = *((ushort *) 0x1F801DC2) * 4; mAPF2 = * ((ushort *) 0x1F801DC2) * 4;
gIIR     = *((short *) 0x1F801DC4) / 32768.0; gIIR = * ((short *) 0x1F801DC4) / 32768.0;
gCOMB1   = *((short *) 0x1F801DC6) / 32768.0; gCOMB1 = * ((short *) 0x1F801DC6) / 32768.0;
gCOMB2   = *((short *) 0x1F801DC8) / 32768.0; gCOMB2 = * ((short *) 0x1F801DC8) / 32768.0;
gCOMB3   = *((short *) 0x1F801DCA) / 32768.0; gCOMB3 = * ((short *) 0x1F801DCA) / 32768.0;
gCOMB4   = *((short *) 0x1F801DCC) / 32768.0; gCOMB4 = * ((short *) 0x1F801DCC) / 32768.0;
gWALL    = *((short *) 0x1F801DCE) / 32768.0; gWALL = * ((short *) 0x1F801DCE) / 32768.0;
gAPF1    = *((short *) 0x1F801DD0) / 32768.0; gAPF1 = * ((short *) 0x1F801DD0) / 32768.0;
gAPF2    = *((short *) 0x1F801DD2) / 32768.0; gAPF2 = * ((short *) 0x1F801DD2) / 32768.0;
z0_Lsame = *((ushort *) 0x1F801DD4) * 4; z0_Lsame = * ((ushort *) 0x1F801DD4) * 4;
z0_Rsame = *((ushort *) 0x1F801DD6) * 4; z0_Rsame = * ((ushort *) 0x1F801DD6) * 4;
m1_Lcomb = *((ushort *) 0x1F801DD8) * 4; m1_Lcomb = * ((ushort *) 0x1F801DD8) * 4;
m1_Rcomb = *((ushort *) 0x1F801DDA) * 4; m1_Rcomb = * ((ushort *) 0x1F801DDA) * 4;
m2_Lcomb = *((ushort *) 0x1F801DDC) * 4; m2_Lcomb = * ((ushort *) 0x1F801DDC) * 4;
m2_Rcomb = *((ushort *) 0x1F801DDE) * 4; m2_Rcomb = * ((ushort *) 0x1F801DDE) * 4;
zm_Lsame = *((ushort *) 0x1F801DE0) * 4; zm_Lsame = * ((ushort *) 0x1F801DE0) * 4;
zm_Rsame = *((ushort *) 0x1F801DE2) * 4; zm_Rsame = * ((ushort *) 0x1F801DE2) * 4;
z0_Ldiff = *((ushort *) 0x1F801DE4) * 4; z0_Ldiff = * ((ushort *) 0x1F801DE4) * 4;
z0_Rdiff = *((ushort *) 0x1F801DE6) * 4; z0_Rdiff = * ((ushort *) 0x1F801DE6) * 4;
m3_Lcomb = *((ushort *) 0x1F801DE8) * 4; m3_Lcomb = * ((ushort *) 0x1F801DE8) * 4;
m3_Rcomb = *((ushort *) 0x1F801DEA) * 4; m3_Rcomb = * ((ushort *) 0x1F801DEA) * 4;
m4_Lcomb = *((ushort *) 0x1F801DEC) * 4; m4_Lcomb = * ((ushort *) 0x1F801DEC) * 4;
m4_Rcomb = *((ushort *) 0x1F801DEE) * 4; m4_Rcomb = * ((ushort *) 0x1F801DEE) * 4;
zm_Ldiff = *((ushort *) 0x1F801DF0) * 4; zm_Ldiff = * ((ushort *) 0x1F801DF0) * 4;
zm_Rdiff = *((ushort *) 0x1F801DF2) * 4; zm_Rdiff = * ((ushort *) 0x1F801DF2) * 4;
z0_Lapf1 = *((ushort *) 0x1F801DF4) * 4; z0_Lapf1 = * ((ushort *) 0x1F801DF4) * 4;
z0_Rapf1 = *((ushort *) 0x1F801DF6) * 4; z0_Rapf1 = * ((ushort *) 0x1F801DF6) * 4;
z0_Lapf2 = *((ushort *) 0x1F801DF8) * 4; z0_Lapf2 = * ((ushort *) 0x1F801DF8) * 4;
z0_Rapf2 = *((ushort *) 0x1F801DFA) * 4; z0_Rapf2 = * ((ushort *) 0x1F801DFA) * 4;
gLIN     = *((short *) 0x1F801DFC) / 32768.0; gLIN = * ((short *) 0x1F801DFC) / 32768.0;
gRIN     = *((short *) 0x1F801DFE) / 32768.0; gRIN = * ((short *) 0x1F801DFE) / 32768.0;
z1_Lsame = z0_Lsame - 1; z1_Lsame = z0_Lsame - 1;
z1_Rsame = z0_Rsame - 1; z1_Rsame = z0_Rsame - 1;
z1_Ldiff = z0_Ldiff - 1; z1_Ldiff = z0_Ldiff - 1;
z1_Rdiff = z0_Rdiff - 1; z1_Rdiff = z0_Rdiff - 1;
zm_Lapf1 = z0_Lapf1 - mAPF1; zm_Lapf1 = z0_Lapf1 - mAPF1;
zm_Rapf1 = z0_Rapf1 - mAPF1; zm_Rapf1 = z0_Rapf1 - mAPF1;
zm_Lapf2 = z0_Lapf2 - mAPF2; zm_Lapf2 = z0_Lapf2 - mAPF2;
zm_Rapf2 = z0_Rapf2 - mAPF2; zm_Rapf2 = z0_Rapf2 - mAPF2;
for (;;) { for (;;) (
/* / *
* LoadFromLowPassFilter?35????39????FIR????     * ???????0???????????35?39???????     */ * LoadFromLowPassFilter 39 or 35 FIR filter taps is equal to 0, * coefficient of the outermost result is the same, or not a 39 or 35 * /
L_in = gLIN * LoadFromLowPassFilterL(); L_in = gLIN * LoadFromLowPassFilterL ();
R_in = gRIN * LoadFromLowPassFilterR(); R_in = gRIN * LoadFromLowPassFilterR ();


/* / *
* Left -> Wall -> Left Reflection * Left -> Wall -> Left Reflection
*/ * /
L_temp = ReadReverbWork(zm_Lsame); L_temp = ReadReverbWork (zm_Lsame);
R_temp = ReadReverbWork(zm_Rsame); R_temp = ReadReverbWork (zm_Rsame);
L_same = L_in + gWALL * L_temp; L_same = L_in + gWALL * L_temp;
R_same = R_in + gWALL * R_temp; R_same = R_in + gWALL * R_temp;
L_temp = ReadReverbWork(z1_Lsame); L_temp = ReadReverbWork (z1_Lsame);
R_temp = ReadReverbWork(z1_Rsame); R_temp = ReadReverbWork (z1_Rsame);
L_same = L_temp + gIIR * (L_same - L_temp); L_same = L_temp + gIIR * (L_same - L_temp);
R_same = R_temp + gIIR * (R_same - R_temp); R_same = R_temp + gIIR * (R_same - R_temp);


/* / *
* Left -> Wall -> Right Reflection * Left -> Wall -> Right Reflection
*/ * /
L_temp = ReadReverbWork(zm_Rdiff); L_temp = ReadReverbWork (zm_Rdiff);
R_temp = ReadReverbWork(zm_Ldiff); R_temp = ReadReverbWork (zm_Ldiff);
L_diff = L_in + gWALL * L_temp; L_diff = L_in + gWALL * L_temp;
R_diff = R_in + gWALL * R_temp; R_diff = R_in + gWALL * R_temp;
L_temp = ReadReverbWork(z1_Ldiff); L_temp = ReadReverbWork (z1_Ldiff);
R_temp = ReadReverbWork(z1_Rdiff); R_temp = ReadReverbWork (z1_Rdiff);
L_diff = L_temp + gIIR * (L_diff - L_temp); L_diff = L_temp + gIIR * (L_diff - L_temp);
R_diff = R_temp + gIIR * (R_diff - R_temp); R_diff = R_temp + gIIR * (R_diff - R_temp);


/* / *
* Early Echo(Comb Filter) * Early Echo (Comb Filter)
*/ * /
L_in = gCOMB1 * ReadReverbWork(m1_Lcomb) + gCOMB2 *ReadReverbWork(m2_Lcomb) + gCOMB3 *ReadReverbWork(m3_Lcomb) + gCOMB4 *ReadReverbWork(m4_Lcomb); L_in = gCOMB1 * ReadReverbWork (m1_Lcomb) + gCOMB2 * ReadReverbWork (m2_Lcomb) + gCOMB3 * ReadReverbWork (m3_Lcomb) + gCOMB4 * ReadReverbWork (m4_Lcomb);
R_in = gCOMB1 * ReadReverbWork(m1_Rcomb) + gCOMB2 *ReadReverbWork(m2_Rcomb) + gCOMB3 *ReadReverbWork(m3_Rcomb) + gCOMB4 *ReadReverbWork(m4_Rcomb); R_in = gCOMB1 * ReadReverbWork (m1_Rcomb) + gCOMB2 * ReadReverbWork (m2_Rcomb) + gCOMB3 * ReadReverbWork (m3_Rcomb) + gCOMB4 * ReadReverbWork (m4_Rcomb);


/* / *
* Late Reverb(Two All Pass Filters) * Late Reverb (Two All Pass Filters)
*/ * /
L_temp = ReadReverbWork(zm_Lapf1); L_temp = ReadReverbWork (zm_Lapf1);
R_temp = ReadReverbWork(zm_Rapf1); R_temp = ReadReverbWork (zm_Rapf1);
L_apf1 = L_in - gAPF1 * L_temp; L_apf1 = L_in - gAPF1 * L_temp;
R_apf1 = R_in - gAPF1 * R_temp; R_apf1 = R_in - gAPF1 * R_temp;
L_in   = L_temp + gAPF1 * L_apf1; L_in = L_temp + gAPF1 * L_apf1;
R_in   = R_temp + gAPF1 * R_apf1; R_in = R_temp + gAPF1 * R_apf1;
L_temp = ReadReverbWork(zm_Lapf2); L_temp = ReadReverbWork (zm_Lapf2);
R_temp = ReadReverbWork(zm_Rapf2); R_temp = ReadReverbWork (zm_Rapf2);
L_apf2 = L_in - gAPF2 * L_temp; L_apf2 = L_in - gAPF2 * L_temp;
R_apf2 = R_in - gAPF2 * R_temp; R_apf2 = R_in - gAPF2 * R_temp;
L_in   = L_temp + gAPF2 * L_apf2; L_in = L_temp + gAPF2 * L_apf2;
R_in   = R_temp + gAPF2 * R_apf2; R_in = R_temp + gAPF2 * R_apf2;


/* / *
* Output * Output
*/ * /
SetOutputL(L_in); SetOutputL (L_in);
SetOutputR(R_in); SetOutputR (R_in);


/* / *
* Write Buffer * Write Buffer
*/ * /
WriteReverbWork(z0_Lsame, L_same); WriteReverbWork (z0_Lsame, L_same);
WriteReverbWork(z0_Rsame, R_same); WriteReverbWork (z0_Rsame, R_same);
WriteReverbWork(z0_Ldiff, L_diff); WriteReverbWork (z0_Ldiff, L_diff);
WriteReverbWork(z0_Rdiff, R_diff); WriteReverbWork (z0_Rdiff, R_diff);
WriteReverbWork(z0_Lapf1, L_apf1); WriteReverbWork (z0_Lapf1, L_apf1);
WriteReverbWork(z0_Rapf1, R_apf1); WriteReverbWork (z0_Rapf1, R_apf1);
WriteReverbWork(z0_Lapf2, L_apf2); WriteReverbWork (z0_Lapf2, L_apf2);
WriteReverbWork(z0_Rapf2, R_apf2); WriteReverbWork (z0_Rapf2, R_apf2);


/* / *
* Update Circular Buffer * Update Circular Buffer
*/ * /
UpdateReverbWork(); UpdateReverbWork ();
} )
#endif



INLINE int MixREVERBLeft(int ns)
{
	if(iUseReverb==0) return 0;
	else
		if(iUseReverb==2)
		{
			if(!rvb.StartAddr)                                  // reverb is off
			{
				rvb.iLastRVBLeft=rvb.iLastRVBRight=rvb.iRVBLeft=rvb.iRVBRight=0;
				return 0;
			}

			iRvbCnt++; iRvbCnt &= 1;

			if(iRvbCnt == 1)                                    // we work on every second left value: downsample to 22 khz
			{
				if(spuCtrl & CTRL_REVERB)                         // -> reverb on? oki
				{
					int ACC0,ACC1,FB_A0,FB_A1,FB_B0,FB_B1;
					
					const int INPUT_SAMPLE_L=*(sRVBStart+(ns<<1));                         
					const int INPUT_SAMPLE_R=*(sRVBStart+(ns<<1)+1);                     
					
					const int IIR_INPUT_A0 = (g_buffer(rvb.IIR_SRC_A0) * rvb.IIR_COEF)/32768L + (INPUT_SAMPLE_L * rvb.IN_COEF_L)/32768L;
					const int IIR_INPUT_A1 = (g_buffer(rvb.IIR_SRC_A1) * rvb.IIR_COEF)/32768L + (INPUT_SAMPLE_R * rvb.IN_COEF_R)/32768L;
					const int IIR_INPUT_B0 = (g_buffer(rvb.IIR_SRC_B0) * rvb.IIR_COEF)/32768L + (INPUT_SAMPLE_L * rvb.IN_COEF_L)/32768L;
					const int IIR_INPUT_B1 = (g_buffer(rvb.IIR_SRC_B1) * rvb.IIR_COEF)/32768L + (INPUT_SAMPLE_R * rvb.IN_COEF_R)/32768L;
					
					const int IIR_A0 = (IIR_INPUT_A0 * rvb.IIR_ALPHA)/32768L + (g_buffer(rvb.IIR_DEST_A0) * (32768L - rvb.IIR_ALPHA))/32768L;
					const int IIR_A1 = (IIR_INPUT_A1 * rvb.IIR_ALPHA)/32768L + (g_buffer(rvb.IIR_DEST_A1) * (32768L - rvb.IIR_ALPHA))/32768L;
					const int IIR_B0 = (IIR_INPUT_B0 * rvb.IIR_ALPHA)/32768L + (g_buffer(rvb.IIR_DEST_B0) * (32768L - rvb.IIR_ALPHA))/32768L;
					const int IIR_B1 = (IIR_INPUT_B1 * rvb.IIR_ALPHA)/32768L + (g_buffer(rvb.IIR_DEST_B1) * (32768L - rvb.IIR_ALPHA))/32768L;
					
					s_buffer1(rvb.IIR_DEST_A0, IIR_A0);
					s_buffer1(rvb.IIR_DEST_A1, IIR_A1);
					s_buffer1(rvb.IIR_DEST_B0, IIR_B0);
					s_buffer1(rvb.IIR_DEST_B1, IIR_B1);
					
					ACC0 = (g_buffer(rvb.ACC_SRC_A0) * rvb.ACC_COEF_A)/32768L +
						(g_buffer(rvb.ACC_SRC_B0) * rvb.ACC_COEF_B)/32768L +
						(g_buffer(rvb.ACC_SRC_C0) * rvb.ACC_COEF_C)/32768L +
						(g_buffer(rvb.ACC_SRC_D0) * rvb.ACC_COEF_D)/32768L;
					ACC1 = (g_buffer(rvb.ACC_SRC_A1) * rvb.ACC_COEF_A)/32768L +
						(g_buffer(rvb.ACC_SRC_B1) * rvb.ACC_COEF_B)/32768L +
						(g_buffer(rvb.ACC_SRC_C1) * rvb.ACC_COEF_C)/32768L +
						(g_buffer(rvb.ACC_SRC_D1) * rvb.ACC_COEF_D)/32768L;
					
					FB_A0 = g_buffer(rvb.MIX_DEST_A0 - rvb.FB_SRC_A);
					FB_A1 = g_buffer(rvb.MIX_DEST_A1 - rvb.FB_SRC_A);
					FB_B0 = g_buffer(rvb.MIX_DEST_B0 - rvb.FB_SRC_B);
					FB_B1 = g_buffer(rvb.MIX_DEST_B1 - rvb.FB_SRC_B);
					
					s_buffer(rvb.MIX_DEST_A0, ACC0 - (FB_A0 * rvb.FB_ALPHA)/32768L);
					s_buffer(rvb.MIX_DEST_A1, ACC1 - (FB_A1 * rvb.FB_ALPHA)/32768L);
					
					s_buffer(rvb.MIX_DEST_B0, (rvb.FB_ALPHA * ACC0)/32768L - (FB_A0 * (int)(rvb.FB_ALPHA^0xFFFF8000))/32768L - (FB_B0 * rvb.FB_X)/32768L);
					s_buffer(rvb.MIX_DEST_B1, (rvb.FB_ALPHA * ACC1)/32768L - (FB_A1 * (int)(rvb.FB_ALPHA^0xFFFF8000))/32768L - (FB_B1 * rvb.FB_X)/32768L);
					
					// save last position for lerp (linear interpolation)
					rvb.iLastRVBLeft  = rvb.iRVBLeft;
					rvb.iLastRVBRight = rvb.iRVBRight;
					
					// Neill - guessed at 0.333
					// Final Fantasy - use 0.38+ for more bass
					rvb.iRVBLeft  = CLAMP16( reverb_target * (g_buffer(rvb.MIX_DEST_A0)+g_buffer(rvb.MIX_DEST_B0))/100 );
					rvb.iRVBRight = CLAMP16( reverb_target * (g_buffer(rvb.MIX_DEST_A1)+g_buffer(rvb.MIX_DEST_B1))/100 );
				}
				else                                              // -> reverb off
				{
					// Vib Ribbon - grab current reverb sample (cdda data)
					// - mono data

					rvb.iLastRVBLeft = rvb.iRVBLeft;
					rvb.iLastRVBLeft = rvb.iRVBRight;

					rvb.iRVBLeft = (short) spuMem[ rvb.CurrAddr ];
					rvb.iRVBRight = rvb.iRVBLeft;
				}


				// Resident Evil 2 - reverb on hall door locks ($4000)
				{
					int voll, volr;


					if( rvb.VolLeft & 0x8000 ) voll = ( rvb.VolLeft & 0x7fff ) - 0x8000;
					else voll = ( rvb.VolLeft & 0x7fff );

					if( rvb.VolRight & 0x8000 ) volr = ( rvb.VolRight & 0x7fff ) - 0x8000;
					else volr = ( rvb.VolRight & 0x7fff );


					rvb.iRVBLeft  = ( rvb.iRVBLeft  * voll ) / 0x8000;
					rvb.iRVBRight = ( rvb.iRVBRight * volr ) / 0x8000;
				}


				Check_IRQ( rvb.CurrAddr*2, 0 );

				rvb.CurrAddr++;
				if(rvb.CurrAddr>0x3ffff) rvb.CurrAddr=rvb.StartAddr;

				// spos = 0x0000
				return CLAMP16( rvb.iLastRVBLeft );
			}
			else
			{
				// spos = 0x8000
				return CLAMP16( rvb.iLastRVBLeft + (rvb.iRVBLeft-rvb.iLastRVBLeft)/2 );
			}
		}
		else                                                  // easy fake reverb:
		{
			const int iRV=*sRVBPlay;                            // -> simply take the reverb mix buf value
			*sRVBPlay++=0;                                      // -> init it after
			if(sRVBPlay>=sRVBEnd) sRVBPlay=sRVBStart;           // -> and take care about wrap arounds
			return CLAMP16(iRV);                                         // -> return reverb mix buf val
		}
}

////////////////////////////////////////////////////////////////////////

INLINE int MixREVERBRight(void)
{
	if(iUseReverb==0) return 0;
	else
		if(iUseReverb==2)                                     // Neill's reverb:
		{
			if( iRvbCnt == 1 ) {
				// spos = 0x0000
				return CLAMP16( rvb.iLastRVBRight );
			}
			else {
				// spos = 0x8000
				return CLAMP16( rvb.iLastRVBRight + (rvb.iRVBRight-rvb.iLastRVBRight)/2 );
			}
		}
		else                                                  // easy fake reverb:
		{
			const int iRV=*sRVBPlay;                            // -> simply take the reverb mix buf value
			*sRVBPlay++=0;                                      // -> init it after
			if(sRVBPlay>=sRVBEnd) sRVBPlay=sRVBStart;           // -> and take care about wrap arounds
			return CLAMP16(iRV);                                         // -> return reverb mix buf val
		}
}

////////////////////////////////////////////////////////////////////////

#endif

/*
-----------------------------------------------------------------------------
PSX reverb hardware notes
by Neill Corlett
-----------------------------------------------------------------------------

	Yadda yadda disclaimer yadda probably not perfect yadda well it's okay anyway
	yadda yadda.
	
		-----------------------------------------------------------------------------
		
			Basics
			------
			
				- The reverb buffer is 22khz 16-bit mono PCM.
				- It starts at the reverb address given by 1DA2, extends to
				the end of sound RAM, and wraps back to the 1DA2 address.
				
					Setting the address at 1DA2 resets the current reverb work address.
					
						This work address ALWAYS increments every 1/22050 sec., regardless of
						whether reverb is enabled (bit 7 of 1DAA set).
						
							And the contents of the reverb buffer ALWAYS play, scaled by the
							"reverberation depth left/right" volumes (1D84/1D86).
							(which, by the way, appear to be scaled so 3FFF=approx. 1.0, 4000=-1.0)
							
								-----------------------------------------------------------------------------
								
									Register names
									--------------
									
										These are probably not their real names.
										These are probably not even correct names.
										We will use them anyway, because we can.
										
											1DC0: FB_SRC_A       (offset)
											1DC2: FB_SRC_B       (offset)
											1DC4: IIR_ALPHA      (coef.)
											1DC6: ACC_COEF_A     (coef.)
											1DC8: ACC_COEF_B     (coef.)
											1DCA: ACC_COEF_C     (coef.)
											1DCC: ACC_COEF_D     (coef.)
											1DCE: IIR_COEF       (coef.)
											1DD0: FB_ALPHA       (coef.)
											1DD2: FB_X           (coef.)
											1DD4: IIR_DEST_A0    (offset)
											1DD6: IIR_DEST_A1    (offset)
											1DD8: ACC_SRC_A0     (offset)
											1DDA: ACC_SRC_A1     (offset)
											1DDC: ACC_SRC_B0     (offset)
											1DDE: ACC_SRC_B1     (offset)
											1DE0: IIR_SRC_A0     (offset)
											1DE2: IIR_SRC_A1     (offset)
											1DE4: IIR_DEST_B0    (offset)
											1DE6: IIR_DEST_B1    (offset)
											1DE8: ACC_SRC_C0     (offset)
											1DEA: ACC_SRC_C1     (offset)
											1DEC: ACC_SRC_D0     (offset)
											1DEE: ACC_SRC_D1     (offset)
											1DF0: IIR_SRC_B1     (offset)
											1DF2: IIR_SRC_B0     (offset)
											1DF4: MIX_DEST_A0    (offset)
											1DF6: MIX_DEST_A1    (offset)
											1DF8: MIX_DEST_B0    (offset)
											1DFA: MIX_DEST_B1    (offset)
											1DFC: IN_COEF_L      (coef.)
											1DFE: IN_COEF_R      (coef.)
											
												The coefficients are signed fractional values.
												-32768 would be -1.0
												32768 would be  1.0 (if it were possible... the highest is of course 32767)
												
													The offsets are (byte/8) offsets into the reverb buffer.
													i.e. you multiply them by 8, you get byte offsets.
													You can also think of them as (samples/4) offsets.
													They appear to be signed.  They can be negative.
													None of the documented presets make them negative, though.
													
														Yes, 1DF0 and 1DF2 appear to be backwards.  Not a typo.
														
															-----------------------------------------------------------------------------
															
																What it does
																------------
																
																	We take all reverb sources:
																	- regular channels that have the reverb bit on
																	- cd and external sources, if their reverb bits are on
																	and mix them into one stereo 44100hz signal.
																	
																		Lowpass/downsample that to 22050hz.  The PSX uses a proper bandlimiting
																		algorithm here, but I haven't figured out the hysterically exact specifics.
																		I use an 8-tap filter with these coefficients, which are nice but probably
																		not the real ones:
																		
																			0.037828187894
																			0.157538631280
																			0.321159685278
																			0.449322115345
																			0.449322115345
																			0.321159685278
																			0.157538631280
																			0.037828187894
																			
																				So we have two input samples (INPUT_SAMPLE_L, INPUT_SAMPLE_R) every 22050hz.
																				
																					* IN MY EMULATION, I divide these by 2 to make it clip less.
																					(and of course the L/R output coefficients are adjusted to compensate)
																					The real thing appears to not do this.
																					
																						At every 22050hz tick:
																						- If the reverb bit is enabled (bit 7 of 1DAA), execute the reverb
																						steady-state algorithm described below
																						- AFTERWARDS, retrieve the "wet out" L and R samples from the reverb buffer
																						(This part may not be exactly right and I guessed at the coefs. TODO: check later.)
																						L is: 0.333 * (buffer[MIX_DEST_A0] + buffer[MIX_DEST_B0])
																						R is: 0.333 * (buffer[MIX_DEST_A1] + buffer[MIX_DEST_B1])
																						- Advance the current buffer position by 1 sample
																						
																							The wet out L and R are then upsampled to 44100hz and played at the
																							"reverberation depth left/right" (1D84/1D86) volume, independent of the main
																							volume.
																							
																								-----------------------------------------------------------------------------
																								
																									Reverb steady-state
																									-------------------
																									
																										The reverb steady-state algorithm is fairly clever, and of course by
																										"clever" I mean "batshit insane".
																										
																											buffer[x] is relative to the current buffer position, not the beginning of
																											the buffer.  Note that all buffer offsets must wrap around so they're
																											contained within the reverb work area.
																											
																												Clipping is performed at the end... maybe also sooner, but definitely at
																												the end.
																												
																													IIR_INPUT_A0 = buffer[IIR_SRC_A0] * IIR_COEF + INPUT_SAMPLE_L * IN_COEF_L;
																													IIR_INPUT_A1 = buffer[IIR_SRC_A1] * IIR_COEF + INPUT_SAMPLE_R * IN_COEF_R;
																													IIR_INPUT_B0 = buffer[IIR_SRC_B0] * IIR_COEF + INPUT_SAMPLE_L * IN_COEF_L;
																													IIR_INPUT_B1 = buffer[IIR_SRC_B1] * IIR_COEF + INPUT_SAMPLE_R * IN_COEF_R;
																													
																														IIR_A0 = IIR_INPUT_A0 * IIR_ALPHA + buffer[IIR_DEST_A0] * (1.0 - IIR_ALPHA);
																														IIR_A1 = IIR_INPUT_A1 * IIR_ALPHA + buffer[IIR_DEST_A1] * (1.0 - IIR_ALPHA);
																														IIR_B0 = IIR_INPUT_B0 * IIR_ALPHA + buffer[IIR_DEST_B0] * (1.0 - IIR_ALPHA);
																														IIR_B1 = IIR_INPUT_B1 * IIR_ALPHA + buffer[IIR_DEST_B1] * (1.0 - IIR_ALPHA);
																														
																															buffer[IIR_DEST_A0 + 1sample] = IIR_A0;
																															buffer[IIR_DEST_A1 + 1sample] = IIR_A1;
																															buffer[IIR_DEST_B0 + 1sample] = IIR_B0;
																															buffer[IIR_DEST_B1 + 1sample] = IIR_B1;
																															
																																ACC0 = buffer[ACC_SRC_A0] * ACC_COEF_A +
																																buffer[ACC_SRC_B0] * ACC_COEF_B +
																																buffer[ACC_SRC_C0] * ACC_COEF_C +
																																buffer[ACC_SRC_D0] * ACC_COEF_D;
																																ACC1 = buffer[ACC_SRC_A1] * ACC_COEF_A +
																																buffer[ACC_SRC_B1] * ACC_COEF_B +
																																buffer[ACC_SRC_C1] * ACC_COEF_C +
																																buffer[ACC_SRC_D1] * ACC_COEF_D;
																																
																																	FB_A0 = buffer[MIX_DEST_A0 - FB_SRC_A];
																																	FB_A1 = buffer[MIX_DEST_A1 - FB_SRC_A];
																																	FB_B0 = buffer[MIX_DEST_B0 - FB_SRC_B];
																																	FB_B1 = buffer[MIX_DEST_B1 - FB_SRC_B];
																																	
																																		buffer[MIX_DEST_A0] = ACC0 - FB_A0 * FB_ALPHA;
																																		buffer[MIX_DEST_A1] = ACC1 - FB_A1 * FB_ALPHA;
																																		buffer[MIX_DEST_B0] = (FB_ALPHA * ACC0) - FB_A0 * (FB_ALPHA^0x8000) - FB_B0 * FB_X;
																																		buffer[MIX_DEST_B1] = (FB_ALPHA * ACC1) - FB_A1 * (FB_ALPHA^0x8000) - FB_B1 * FB_X;
																																		
																																			-----------------------------------------------------------------------------
*/

