/***************************************************************************
spu.c  -	description
-------------------
begin 							 : Wed May 15 2002
copyright 					 : (C) 2002 by Pete Bernert
email 							 : BlackDove@addcom.de
***************************************************************************/

/***************************************************************************
* 																																				*
* 	This program is free software; you can redistribute it and/or modify	*
* 	it under the terms of the GNU General Public License as published by	*
* 	the Free Software Foundation; either version 2 of the License, or 		*
* 	(at your option) any later version. See also the license.txt file for *
* 	additional informations.																							*
* 																																				*
***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2004/09/19 - Pete
// - added option: IRQ handling in the decoded sound buffer areas (Crash Team Racing)
//
// 2004/09/18 - Pete
// - changed global channel var handling to local pointers (hopefully it will help LDChen's port)
//
// 2004/04/22 - Pete
// - finally fixed frequency modulation and made some cleanups
//
// 2003/04/07 - Eric
// - adjusted cubic interpolation algorithm
//
// 2003/03/16 - Eric
// - added cubic interpolation
//
// 2003/03/01 - linuzappz
// - libraryName changes using ALSA
//
// 2003/02/28 - Pete
// - added option for type of interpolation
// - adjusted spu irqs again (Thousant Arms, Valkyrie Profile)
// - added MONO support for MSWindows DirectSound
//
// 2003/02/20 - kode54
// - amended interpolation code, goto GOON could skip initialization of gpos and cause segfault
//
// 2003/02/19 - kode54
// - moved SPU IRQ handler and changed sample flag processing
//
// 2003/02/18 - kode54
// - moved ADSR calculation outside of the sample decode loop, somehow I doubt that
//	 ADSR timing is relative to the frequency at which a sample is played... I guess
//	 this remains to be seen, and I don't know whether ADSR is applied to noise channels...
//
// 2003/02/09 - kode54
// - one-shot samples now process the end block before stopping
// - in light of removing fmod hack, now processing ADSR on frequency channel as well
//
// 2003/02/08 - kode54
// - replaced easy interpolation with gaussian
// - removed fmod averaging hack
// - changed .sinc to be updated from .iRawPitch, no idea why it wasn't done this way already (<- Pete: because I sometimes fail to see the obvious, haharhar :)
//
// 2003/02/08 - linuzappz
// - small bugfix for one usleep that was 1 instead of 1000
// - added iDisStereo for no stereo (Linux)
//
// 2003/01/22 - Pete
// - added easy interpolation & small noise adjustments
//
// 2003/01/19 - Pete
// - added Neill's reverb
//
// 2003/01/12 - Pete
// - added recording window handlers
//
// 2003/01/06 - Pete
// - added Neill's ADSR timings
//
// 2002/12/28 - Pete
// - adjusted spu irq handling, fmod handling and loop handling
//
// 2002/08/14 - Pete
// - added extra reverb
//
// 2002/06/08 - linuzappz
// - SPUupdate changed for SPUasync
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"

#define _IN_SPU

#include "externals.h"
#include "cfg.h"
#include "dsoundoss.h"
#include "xaudio_2.h"
#include "regs.h"
#include "debug.h"
#include "record.h"
#include "resource.h"
#include "registers.h"



// new stuff
int phantom_padder;
int phantom_pad_size;
int phantom_post_pad;
int upload_timer;
int latency_target;
int latency_restart;
int upload_low_reset;
int upload_high_full;
int upload_high_reset;
int async_wait_block;
int async_ondemand_block;
int debug_sound_buffer;
int debug_cdxa_buffer;
int sound_stretcher;
int reverb_target;


#include <stdio.h>
extern FILE *fp_spu_log;




// multi-threading
int thread_async_ended;


// Dekker's algorithm
int mutex_flag[2];

#define MUTEX_SPU 0
#define MUTEX_PHANTOM 1


////////////////////////////////////////////////////////////////////////
// spu version infos/name
////////////////////////////////////////////////////////////////////////

const unsigned char version  = 1;
const unsigned char revision = 1;
const unsigned char build 	 = 10;				 
#ifdef _WINDOWS 				 
static char * libraryName 		= "P.E.Op.S. Sound Audio Driver";
#else
#ifndef USEALSA
static char * libraryName 		= "P.E.Op.S. OSS Audio Driver";
#else
static char * libraryName 		= "P.E.Op.S. ALSA Audio Driver";
#endif
#endif															
static char * libraryInfo 		= "P.E.Op.S. OSS Driver V1.9\nCoded by Pete Bernert and the P.E.Op.S. team\n";

////////////////////////////////////////////////////////////////////////
// globals
////////////////////////////////////////////////////////////////////////

int spu_ch;



// psx buffer / addresses

unsigned short	regArea[10000]; 											 
unsigned short	spuMem[256*1024];
unsigned char * spuMemC;
unsigned char * pSpuIrq;
unsigned char * pSpuBuffer;
unsigned char * pMixIrq=0;


// user settings					

int 						iOutputDriver=0;
int 						iUseXA=1;
int 						iVolume=10;
int 						iXAPitch=0;
int 						iUseTimer=4;
int 						iSPUIRQWait=0;
int 						iDebugMode=0;
int 						iRecordMode=0;
int 						iUseReverb=2;
int 						iUseInterpolation=2;
int 						iDisStereo=0;
int 						iUseDBufIrq=0;
int 						iFreqResponse=0;
int 						iEmuType=0;
int 						iReleaseIrq=0;
int 						iReverbBoost=0;

int 						iLatency=4;
int 						iXAInterp=0;
int 						iCDDAInterp=0;
int 						iOutputInterp1=0;
int 						iOutputInterp2=0;
int 						iVolCDDA=10;
int 						iVolXA=10;
int 						iVolVoices=10;

int 						iXAStrength = 4;
int 						iCDDAStrength = 4;
int 						iOutput2Strength = 4;

// MAIN infos struct for each channel

SPUCHAN 				s_chan[MAXCHAN+1];										 // channel + 1 infos (1 is security for fmod handling)
REVERBInfo			rvb;

unsigned long 	dwNoiseVal=1; 												 // global noise generator
unsigned long 	dwNoiseClock=0; 											 // global noise generator
unsigned long 	dwNoiseCount=0; 											 // global noise generator

unsigned short	spuCtrl=0;														 // some vars to store psx reg infos
unsigned short	spuStat=0;
unsigned short	spuIrq=0; 						
unsigned long 	spuAddr=0xffffffff; 									 // address into spu mem
int 						bEndThread=0; 												 // thread handlers
int 						bThreadEnded=0;
int 						bSpuInit=0;
int 						bSPUIsOpen=0;

int 						decoded_ptr = 0;
int 						bIrqHit = 0;

int 						iVolMainL = 0;
int 						iVolMainR = 0;

long cpu_cycles;
long cpu_clock;
long total_cpu_cycles;
long total_apu_cycles;

long APU_run = 10;

int framelimiter = 1;



#ifdef _WINDOWS
HWND		hWMain=0; 																		 // window handle
HWND		hWDebug=0;
HWND		hWRecord=0;
static HANDLE 	hMainThread;													 
#else
// 2003/06/07 - Pete
#ifndef NOTHREADLIB
static pthread_t thread = -1; 												 // thread id (linux)
#endif
#endif

unsigned long dwNewChannel=0; 												 // flags for faster testing, if new channel starts

void (CALLBACK *irqCallback)(void)=0; 								 // func of main emu, called on spu irq
void (CALLBACK *cddavCallback)(unsigned short,unsigned short)=0;
void (CALLBACK *irqQSound)(unsigned char *,long *,long)=0;			

// certain globals (were local before, but with the new timeproc I need em global)

const int f[5][2] = {
	{ 	 0,  0	},
	{ 	60,  0	},
	{  115, -52 },
	{ 	98, -55 },
	{  122, -60 }
};

int SSumR[NSSIZE];
int SSumL[NSSIZE];
int iFMod[NSSIZE];
int iCycle=0;
short * pS;

static int lastch=-1; 		 // last channel processed on spu irq in timer mode
static int lastns=0;			 // last ns pos
static int iSecureStart=0; // secure start counter

////////////////////////////////////////////////////////////////////////
// CODE AREA
////////////////////////////////////////////////////////////////////////

// dirty inline func includes

#include "reverb.cpp" 			 
#include "adsr.cpp"




#undef SPU_LOG
//#define SPU_LOG



////////////////////////////////////////////////////////////////////////
// helpers for simple interpolation

//
// easy interpolation on upsampling, no special filter, just "Pete's common sense" tm
//
// instead of having n equal sample values in a row like:
//			 ____
//					 |____
//
// we compare the current delta change with the next delta change.
//
// if curr_delta is positive,
//
//	- and next delta is smaller (or changing direction):
//				 \.
//					-__
//
//	- and next delta significant (at least twice) bigger:
//				 --_
//						\.
//
//	- and next delta is nearly same:
//					\.
//					 \.
//
//
// if curr_delta is negative,
//
//	- and next delta is smaller (or changing direction):
//					_--
//				 /
//
//	- and next delta significant (at least twice) bigger:
//						/
//				 __- 
//				 
//	- and next delta is nearly same:
//					 /
//					/
//		 


INLINE void InterpolateUp(SPUCHAN * pChannel)
{
	if(pChannel->SB[32]==1) 															// flag == 1? calc step and set flag... and don't change the value in this pass
	{
		const int id1=pChannel->SB[30]-pChannel->SB[29];		// curr delta to next val
		const int id2=pChannel->SB[31]-pChannel->SB[30];		// and next delta to next-next val :)
		
		pChannel->SB[32]=0;
		
		if(id1>0) 																					// curr delta positive
		{
			if(id2<id1)
			{pChannel->SB[28]=id1;pChannel->SB[32]=2;}
			else
				if(id2<(id1<<1))
					pChannel->SB[28]=(id1*pChannel->sinc)/0x10000L;
				else
					pChannel->SB[28]=(id1*pChannel->sinc)/0x20000L; 
		}
		else																								// curr delta negative
		{
			if(id2>id1)
			{pChannel->SB[28]=id1;pChannel->SB[32]=2;}
			else
				if(id2>(id1<<1))
					pChannel->SB[28]=(id1*pChannel->sinc)/0x10000L;
				else
					pChannel->SB[28]=(id1*pChannel->sinc)/0x20000L; 
		}
	}
	else
		if(pChannel->SB[32]==2) 															// flag 1: calc step and set flag... and don't change the value in this pass
		{
			pChannel->SB[32]=0;
			
			pChannel->SB[28]=(pChannel->SB[28]*pChannel->sinc)/0x20000L;
			if(pChannel->sinc<=0x8000)
				pChannel->SB[29]=pChannel->SB[30]-(pChannel->SB[28]*((0x10000/pChannel->sinc)-1));
			else pChannel->SB[29]+=pChannel->SB[28];
		}
		else																									// no flags? add bigger val (if possible), calc smaller step, set flag1
			pChannel->SB[29]+=pChannel->SB[28];
}

//
// even easier interpolation on downsampling, also no special filter, again just "Pete's common sense" tm
//

INLINE void InterpolateDown(SPUCHAN * pChannel)
{
	if(pChannel->sinc>=0x20000L)																// we would skip at least one val?
	{
		pChannel->SB[29]+=(pChannel->SB[30]-pChannel->SB[29])/2;	// add easy weight
		if(pChannel->sinc>=0x30000L)															// we would skip even more vals?
			pChannel->SB[29]+=(pChannel->SB[31]-pChannel->SB[30])/2; // add additional next weight
	}
}

////////////////////////////////////////////////////////////////////////
// helpers for gauss interpolation

#define gval0 (((int*)(&pChannel->SB[29]))[gpos])
#define gval(x) (((int*)(&pChannel->SB[29]))[(gpos+x)&3])

#include "gauss_i.h"




int out_gauss_ptr = 0;
int out_gauss_window[8] = {0, 0, 0, 0, 0, 0, 0, 0};

#define out_gvall0 out_gauss_window[out_gauss_ptr]
#define out_gvall(x) out_gauss_window[(out_gauss_ptr+x)&3]
#define out_gvalr0 out_gauss_window[4+out_gauss_ptr]
#define out_gvalr(x) out_gauss_window[4+((out_gauss_ptr+x)&3)]




static int pete_simple_l[5];
static int pete_simple_r[5];

#define pete_svall(x) pete_simple_l[x-28]
#define pete_svalr(x) pete_simple_r[x-28]


INLINE void OutSimpleInterpolateUpL(int sinc)
{
	if( sinc == 0 ) sinc=0x10000;
	
	
	if(pete_svall(32)==1) 															// flag == 1? calc step and set flag... and don't change the value in this pass
	{
		const int id1=pete_svall(30)-pete_svall(29);		// curr delta to next val
		const int id2=pete_svall(31)-pete_svall(30);		// and next delta to next-next val :)
		
		pete_svall(32)=0;
		
		if(id1>0) 																					// curr delta positive
		{
			if(id2<id1)
			{pete_svall(28)=id1;pete_svall(32)=2;}
			else
				if(id2<(id1<<1))
					pete_svall(28)=(id1*sinc)/0x10000L;
				else
					pete_svall(28)=(id1*sinc)/0x20000L; 
		}
		else																								// curr delta negative
		{
			if(id2>id1)
			{pete_svall(28)=id1;pete_svall(32)=2;}
			else
				if(id2>(id1<<1))
					pete_svall(28)=(id1*sinc)/0x10000L;
				else
					pete_svall(28)=(id1*sinc)/0x20000L; 
		}
	}
	else
		if(pete_svall(32)==2) 															// flag 1: calc step and set flag... and don't change the value in this pass
		{
			pete_svall(32)=0;
			
			pete_svall(28)=(pete_svall(28)*sinc)/0x20000L;
			if(sinc<=0x8000)
				pete_svall(29)=pete_svall(30)-(pete_svall(28)*((0x10000/sinc)-1));
			else pete_svall(29)+=pete_svall(28);
		}
		else																									// no flags? add bigger val (if possible), calc smaller step, set flag1
			pete_svall(29)+=pete_svall(28);
}


INLINE void OutSimpleInterpolateUpR(int sinc)
{
	if( sinc == 0 ) sinc=0x10000;
	
	
	if(pete_svalr(32)==1) 															// flag == 1? calc step and set flag... and don't change the value in this pass
	{
		const int id1=pete_svalr(30)-pete_svalr(29);		// curr delta to next val
		const int id2=pete_svalr(31)-pete_svalr(30);		// and next delta to next-next val :)
		
		pete_svalr(32)=0;
		
		if(id1>0) 																					// curr delta positive
		{
			if(id2<id1)
			{pete_svalr(28)=id1;pete_svalr(32)=2;}
			else
				if(id2<(id1<<1))
					pete_svalr(28)=(id1*sinc)/0x10000L;
				else
					pete_svalr(28)=(id1*sinc)/0x20000L; 
		}
		else																								// curr delta negative
		{
			if(id2>id1)
			{pete_svalr(28)=id1;pete_svalr(32)=2;}
			else
				if(id2>(id1<<1))
					pete_svalr(28)=(id1*sinc)/0x10000L;
				else
					pete_svalr(28)=(id1*sinc)/0x20000L; 
		}
	}
	else
		if(pete_svalr(32)==2) 															// flag 1: calc step and set flag... and don't change the value in this pass
		{
			pete_svalr(32)=0;
			
			pete_svalr(28)=(pete_svalr(28)*sinc)/0x20000L;
			if(sinc<=0x8000)
				pete_svalr(29)=pete_svalr(30)-(pete_svalr(28)*((0x10000/sinc)-1));
			else pete_svalr(29)+=pete_svall(28);
		}
		else																									// no flags? add bigger val (if possible), calc smaller step, set flag1
			pete_svalr(29)+=pete_svalr(28);
}



INLINE void OutGetInterpolationVal(int spos, int sinc, int *out_lc, int *out_rc)
{
	int lc,rc;
	
	
	switch(iOutputInterp2)
	{
	case 7:
		{
			// SPU2-X - Catmull-Rom (PCSX2 team)
			// - desharpen
			
			int y3,y2,y1,y0,mu;
			int l_val, r_val;
			
			int l_a3, l_a2, l_a1, l_a0;
			int r_a3, r_a2, r_a1, r_a0;
			
			mu = spos;
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = out_gvall(0);
			y1 = out_gvall(1);
			y2 = out_gvall(2);
			y3 = out_gvall(3);
			
			l_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			l_a2 = ( 2*y0 - 3*y1 + 3*y2 - y3)>>0;
			l_a1 = (-  y0 			 +	 y2 		)>>0;
			l_a0 = (				2*y1						)>>0;
			
			// mu = 0.16 (fixed-point)
			l_val = ((l_a3	) * mu) >> 16;
			l_val = ((l_a2 + l_val) * mu) >> 16;
			l_val = ((l_a1 + l_val) * mu) >> 16;
			
			lc = l_a0 + (l_val>>0);
			
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = out_gvalr(0);
			y1 = out_gvalr(1);
			y2 = out_gvalr(2);
			y3 = out_gvalr(3);
			
			r_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			r_a2 = ( 2*y0 - 3*y1 + 3*y2 - y3)>>0;
			r_a1 = (-  y0 			 +	 y2 		)>>0;
			r_a0 = (				2*y1						)>>0;
			
			r_val = ((r_a3	) * mu) >> 16;
			r_val = ((r_a2 + r_val) * mu) >> 16;
			r_val = ((r_a1 + r_val) * mu) >> 16;
			
			rc = r_a0 + (r_val>>0);
			
			
			// over-volume adjust
			lc /= 2;
			rc /= 2;
		} break;
		//--------------------------------------------------//
	case 6:
		{
			// SPU2-X - Catmull-Rom (PCSX2 team)
			
			int y3,y2,y1,y0,mu;
			int l_val, r_val;
			
			int l_a3, l_a2, l_a1, l_a0;
			int r_a3, r_a2, r_a1, r_a0;
			
			mu = spos;
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = out_gvall(0);
			y1 = out_gvall(1);
			y2 = out_gvall(2);
			y3 = out_gvall(3);
			
			l_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			l_a2 = ( 2*y0 - 5*y1 + 4*y2 - y3)>>0;
			l_a1 = (-  y0 			 +	 y2 		)>>0;
			l_a0 = (				2*y1						)>>0;
			
			// mu = 0.16 (fixed-point)
			l_val = ((l_a3	) * mu) >> 16;
			l_val = ((l_a2 + l_val) * mu) >> 16;
			l_val = ((l_a1 + l_val) * mu) >> 16;
			
			lc = l_a0 + (l_val>>0);
			
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = out_gvalr(0);
			y1 = out_gvalr(1);
			y2 = out_gvalr(2);
			y3 = out_gvalr(3);
			
			r_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			r_a2 = ( 2*y0 - 5*y1 + 4*y2 - y3)>>0;
			r_a1 = (-  y0 			 +	 y2 		)>>0;
			r_a0 = (				2*y1						)>>0;
			
			r_val = ((r_a3	) * mu) >> 16;
			r_val = ((r_a2 + r_val) * mu) >> 16;
			r_val = ((r_a1 + r_val) * mu) >> 16;
			
			rc = r_a0 + (r_val>>0);
			
			
			// over-volume adjust
			lc /= 2;
			rc /= 2;
		} break;
		//--------------------------------------------------//
	case 5:
		{
			// SPU2-X - Hermite (PCSX2 team)
			
			int y3,y2,y1,y0,mu;
			int l_val, r_val;
			
			int l_m00,l_m01,l_m0,l_m10,l_m11,l_m1;
			int r_m00,r_m01,r_m0,r_m10,r_m11,r_m1;
			
			mu = spos;			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = out_gvall(0);
			y1 = out_gvall(1);
			y2 = out_gvall(2);
			y3 = out_gvall(3);
			
			l_m00 = ((y1-y0)*HERMITE_TENSION) >> 16; // 16.0
			l_m01 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			l_m0	= l_m00 + l_m01;
			
			l_m10 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			l_m11 = ((y3-y2)*HERMITE_TENSION) >> 16; // 16.0
			l_m1	= l_m10 + l_m11;
			
			l_val = ((	2*y1 +	 l_m0 + l_m1 - 2*y2) * mu) >> 16; // 16.0
			l_val = ((l_val - 3*y1 - 2*l_m0 - l_m1 + 3*y2) * mu) >> 16; // 16.0
			l_val = ((l_val 			 +	 l_m0 					 ) * mu) >> 16; // 16.0
			
			lc = l_val + (y1>>0);
			
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = out_gvalr(0);
			y1 = out_gvalr(1);
			y2 = out_gvalr(2);
			y3 = out_gvalr(3);
			
			r_m00 = ((y1-y0)*HERMITE_TENSION) >> 16; // 16.0
			r_m01 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			r_m0	= r_m00 + r_m01;
			
			r_m10 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			r_m11 = ((y3-y2)*HERMITE_TENSION) >> 16; // 16.0
			r_m1	= r_m10 + r_m11;
			
			r_val = ((	2*y1 +	 r_m0 + r_m1 - 2*y2) * mu) >> 16; // 16.0
			r_val = ((r_val - 3*y1 - 2*r_m0 - r_m1 + 3*y2) * mu) >> 16; // 16.0
			r_val = ((r_val 			 +	 r_m0 					 ) * mu) >> 16; // 16.0
			
			rc = r_val + (y1>>0);
		} break;
		//--------------------------------------------------//
	case 4: 																						// cubic interpolation
		{
			int xd;
			
			xd = (spos >> 1)+1;
			
			lc	= out_gvall(3) - 3*out_gvall(2) + 3*out_gvall(1) - out_gvall(0);
			lc *= (xd - (2<<15)) / 6;
			lc >>= 15;
			lc += out_gvall(2) - out_gvall(1) - out_gvall(1) + out_gvall(0);
			lc *= (xd - (1<<15)) >> 1;
			lc >>= 15;
			lc += out_gvall(1) - out_gvall(0);
			lc *= xd;
			lc >>= 15;
			lc = lc + out_gvall(0);
			
			
			rc	= out_gvalr(3) - 3*out_gvalr(2) + 3*out_gvalr(1) - out_gvalr(0);
			rc *= (xd - (2<<15)) / 6;
			rc >>= 15;
			rc += out_gvalr(2) - out_gvalr(1) - out_gvalr(1) + out_gvalr(0);
			rc *= (xd - (1<<15)) >> 1;
			rc >>= 15;
			rc += out_gvalr(1) - out_gvalr(0);
			rc *= xd;
			rc >>= 15;
			rc = rc + out_gvalr(0);
			
		} break;
		//--------------------------------------------------//
	 case 3:
		 {
		 /*
		 ADPCM interpolation (4-tap FIR)
		 
			 y[n] = (x[n-3] * 4807 + x[n-2] * 22963 + x[n-1] * 4871 - x[n]) >> 15;
			 
				 - Dr. Hell (Xebra PS1 emu)
			 */
			 
			 lc = (out_gvall(3) * 4807 + out_gvall(2) * 22963 + out_gvall(1) * 4871 - out_gvall(0)) >> 15;
			 rc = (out_gvalr(3) * 4807 + out_gvalr(2) * 22963 + out_gvalr(1) * 4871 - out_gvalr(0)) >> 15;
		 } break;
		 //--------------------------------------------------//
	 case 2:																						 // gauss interpolation
		 {
			 int vl, vr;
			 
			 // safety check
			 spos &= 0xffff;	
			 
			 spos = (spos >> 6) & ~3;
			 
			 vl=(gauss[spos]*out_gvall(0)) >> 15;
			 vl+=(gauss[spos+1]*out_gvall(1)) >> 15;
			 vl+=(gauss[spos+2]*out_gvall(2)) >> 15;
			 vl+=(gauss[spos+3]*out_gvall(3)) >> 15;
			 
			 vr=(gauss[spos]*out_gvalr(0)) >> 15;
			 vr+=(gauss[spos+1]*out_gvalr(1)) >> 15;
			 vr+=(gauss[spos+2]*out_gvalr(2)) >> 15;
			 vr+=(gauss[spos+3]*out_gvalr(3)) >> 15;
			 
			 lc = vl;
			 rc = vr;
		 } break;
		 //--------------------------------------------------//
	 case 1:																						 // simple interpolation
		 {
			 OutSimpleInterpolateUpL(sinc); 										// --> interpolate up
			 OutSimpleInterpolateUpR(sinc); 										// --> interpolate up
			 
			 lc=pete_svall(29);
			 rc=pete_svalr(29);
		 } break;
		 //--------------------------------------------------//
	 default: 																					 // no interpolation
		 {
			 lc = out_gvall0;
			 rc = out_gvalr0;
		 } break;
		 //--------------------------------------------------//
	}
	
	*out_lc = CLAMP16( lc );
	*out_rc = CLAMP16( rc );
}


INLINE void OutStoreInterpolationVal(int val_l, int val_r)
{
	if((spuCtrl&0x4000)==0) {
		val_l=0;											 // muted?
		val_r=0;
	}
	
	
	out_gvall0 = CLAMP16( val_l );
	out_gvalr0 = CLAMP16( val_r );
	
	
	if(iOutputInterp2>=2) 													 // gauss/cubic interpolation
	{
		out_gauss_ptr = (out_gauss_ptr+1) & 3;
	}
	else
		if(iOutputInterp2==1) 													 // simple interpolation
		{
			pete_svall(28) = 0;
			pete_svall(29) = pete_svall(30);
			pete_svall(30) = pete_svall(31);
			pete_svall(31) = CLAMP16( val_l );
			pete_svall(32) = 1;
			
			pete_svalr(28) = 0;
			pete_svalr(29) = pete_svalr(30);
			pete_svalr(30) = pete_svalr(31);
			pete_svalr(31) = CLAMP16( val_r );
			pete_svalr(32) = 1;
		}
		else {
			//pChannel->SB[29]=fa;													 // no interpolation
		}
}


////////////////////////////////////////////////////////////////////////

#include "xa.cpp"

////////////////////////////////////////////////////////////////////////
// START SOUND... called by main thread to setup a new sound on a channel
////////////////////////////////////////////////////////////////////////

INLINE void StartSound(SPUCHAN * pChannel)
{
	StartADSR(pChannel);
	StartREVERB(pChannel);			
	
	
	// Okay to do here now?
	pChannel->bLoopJump = 0;
	
	pChannel->iSilent=0;
	pChannel->bStop=0;
	pChannel->bOn=1;
	pChannel->pCurr = pChannel->pStart;
	
	
	
	pChannel->s_1=0;																			// init mixing vars
	pChannel->s_2=0;
	pChannel->iSBPos=28;
	
	pChannel->bNew=0; 																		// init channel flags
	
	pChannel->SB[29]=0; 																	// init our interpolation helpers
	pChannel->SB[30]=0;
	
	
	
	if(iUseInterpolation>=2)															// gauss interpolation?
	{pChannel->spos=0x30000L;pChannel->SB[28]=0;} 	 // -> start with more decoding
	else {pChannel->spos=0x10000L;pChannel->SB[31]=0;}		// -> no/simple interpolation starts with one 44100 decoding
}

////////////////////////////////////////////////////////////////////////
// ALL KIND OF HELPERS
////////////////////////////////////////////////////////////////////////

INLINE void VoiceChangeFrequency(SPUCHAN * pChannel)
{
	pChannel->iUsedFreq=pChannel->iActFreq; 							// -> take it and calc steps
	pChannel->sinc=pChannel->iRawPitch<<4;
	if(!pChannel->sinc) pChannel->sinc=1;
	if(iUseInterpolation==1) pChannel->SB[32]=1;					// -> freq change in simle imterpolation mode: set flag
}

////////////////////////////////////////////////////////////////////////

INLINE void FModChangeFrequency(SPUCHAN * pChannel,int ns)
{
	int NP=pChannel->iRawPitch;
	
	NP=((32768L+iFMod[ns])*NP)/32768L;
	
	if(NP>0x3fff) NP=0x3fff;
	if(NP<0x1)		NP=0x1;
	
	NP=(44100L*NP)/(4096L); 															// calc frequency
	
	pChannel->iActFreq=NP;
	pChannel->iUsedFreq=NP;
	pChannel->sinc=(((NP/10)<<16)/4410);
	if(!pChannel->sinc) pChannel->sinc=1;
	if(iUseInterpolation==1) pChannel->SB[32]=1;					// freq change in simple interpolation mode
	
	iFMod[ns]=0;
} 									 

////////////////////////////////////////////////////////////////////////

/*
Noise Algorithm
- Dr.Hell (Xebra PS1 emu)
- http://drhell.web.fc2.com

	
		Level change cycle
		- Freq = 0x8000 >> (NoiseClock >> 2);
		
			
				Frequency of half cycle
				- Half = ((NoiseClock & 3) * 2) / (4 + (NoiseClock & 3));
				
					0 = (0*2)/(4+0) = 0/4
					1 = (1*2)/(4+1) = 2/5
					2 = (2*2)/(4+2) = 4/6
					3 = (3*2)/(4+3) = 6/7
					
						-------------------------------
						
							5*6*7 = 210
							4 -  0*0 = 0
							5 - 42*2 = 84
							6 - 35*4 = 140
							7 - 30*6 = 180
*/

// Noise Waveform - Dr. Hell (Xebra)
char NoiseWaveAdd [64] = {
	1, 0, 0, 1, 0, 1, 1, 0,
		1, 0, 0, 1, 0, 1, 1, 0,
		1, 0, 0, 1, 0, 1, 1, 0,
		1, 0, 0, 1, 0, 1, 1, 0,
		0, 1, 1, 0, 1, 0, 0, 1,
		0, 1, 1, 0, 1, 0, 0, 1,
		0, 1, 1, 0, 1, 0, 0, 1,
		0, 1, 1, 0, 1, 0, 0, 1
};

unsigned short NoiseFreqAdd[5] = {
	0, 84, 140, 180, 210
};


INLINE void NoiseClock()
{
	unsigned int level;
	
	level = 0x8000 >> (dwNoiseClock >> 2);
	level <<= 16;
	
	dwNoiseCount += 0x10000;
	
	
	// Dr. Hell - fraction
	dwNoiseCount += NoiseFreqAdd[ dwNoiseClock & 3 ];
	if( (dwNoiseCount&0xffff) >= NoiseFreqAdd[4] ) {
		dwNoiseCount += 0x10000;
		dwNoiseCount -= NoiseFreqAdd[ dwNoiseClock & 3 ];
	}
	
	
	if( dwNoiseCount >= level )
	{
		while( dwNoiseCount >= level )
			dwNoiseCount -= level;
		
		// Dr. Hell - form
		dwNoiseVal = (dwNoiseVal<<1) | NoiseWaveAdd[ (dwNoiseVal>>10) & 63 ];
	}
}

INLINE int iGetNoiseVal(SPUCHAN * pChannel)
{
	int fa;
	
	fa = (short) dwNoiseVal;
	
	// don't upset VAG decoder
	//if(iUseInterpolation<2) 															// no gauss/cubic interpolation?
	//pChannel->SB[29] = fa;															 // -> store noise val in "current sample" slot
	
	return fa;
} 																

////////////////////////////////////////////////////////////////////////

INLINE void StoreInterpolationVal(SPUCHAN * pChannel,int fa)
{
	fa = CLAMP16(fa);
	
	
	// fmod channel has output too
	{
		if((spuCtrl&0x4000)==0) fa=0; 											// muted?
		else																								// else adjust
		{
			// clip at mixer output, not now
			//if(fa>32767L)  fa=32767L;
			//if(fa<-32768L) fa=-32768L;							
		}
		
		if(iUseInterpolation>=2)														// gauss/cubic interpolation
		{
			int gpos = pChannel->SB[28];
			gval0 = fa;
			gpos = (gpos+1) & 3;
			pChannel->SB[28] = gpos;
		}
		else
		{
			if(iUseInterpolation==1)														// simple interpolation
			{
				pChannel->SB[28] = 0; 									 
				pChannel->SB[29] = pChannel->SB[30];							// -> helpers for simple linear interpolation: delay real val for two slots, and calc the two deltas, for a 'look at the future behaviour'
				pChannel->SB[30] = pChannel->SB[31];
				pChannel->SB[31] = fa;
				pChannel->SB[32] = 1; 														// -> flag: calc new interolation
			}
			else pChannel->SB[29]=fa; 													// no interpolation
		}
	}
}

////////////////////////////////////////////////////////////////////////

INLINE int iGetInterpolationVal(SPUCHAN * pChannel)
{
	int fa;
	
	switch(iUseInterpolation)
	{
	case 7:
		{
			// SPU2-X - Catmull-Rom (PCSX2 team)
			// - desharpen
			
			int gpos;
			
			int y3,y2,y1,y0,mu;
			int l_val;
			
			int l_a3, l_a2, l_a1, l_a0;
			
			mu = pChannel->spos;
			gpos = pChannel->SB[28];
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = gval(0);
			y1 = gval(1);
			y2 = gval(2);
			y3 = gval(3);
			
			l_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			l_a2 = ( 2*y0 - 4*y1 + 3*y2 - y3)>>0;
			l_a1 = (-  y0 			 +	 y2 		)>>0;
			l_a0 = (				2*y1						)>>0;
			
			// mu = 0.16 (fixed-point)
			l_val = ((l_a3	) * mu) >> 16;
			l_val = ((l_a2 + l_val) * mu) >> 16;
			l_val = ((l_a1 + l_val) * mu) >> 16;
			
			fa = l_a0 + (l_val>>0);
			
			
			// over-volume adjust
			fa /= 2;
		} break;
		//--------------------------------------------------//
	case 6:
		{
			// SPU2-X - Catmull-Rom (PCSX2 team)
			
			int gpos;
			
			int y3,y2,y1,y0,mu;
			int l_val;
			
			int l_a3, l_a2, l_a1, l_a0;
			
			mu = pChannel->spos;
			gpos = pChannel->SB[28];
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = gval(0);
			y1 = gval(1);
			y2 = gval(2);
			y3 = gval(3);
			
			l_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			l_a2 = ( 2*y0 - 5*y1 + 4*y2 - y3)>>0;
			l_a1 = (-  y0 			 +	 y2 		)>>0;
			l_a0 = (				2*y1						)>>0;
			
			// mu = 0.16 (fixed-point)
			l_val = ((l_a3	) * mu) >> 16;
			l_val = ((l_a2 + l_val) * mu) >> 16;
			l_val = ((l_a1 + l_val) * mu) >> 16;
			
			fa = l_a0 + (l_val>>0);
			
			
			// over-volume adjust
			fa /= 2;
		} break;
		//--------------------------------------------------//
	case 5:
		{
			// SPU2-X - Hermite (PCSX2 team)
			
			int gpos;
			
			int y3,y2,y1,y0,mu;
			int l_val;
			
			int l_m00,l_m01,l_m0,l_m10,l_m11,l_m1;
			
			mu = pChannel->spos;
			gpos = pChannel->SB[28];
			
			
			// voice more clear this way - Mega Man Legends 2
			y0 = gval(0);
			y1 = gval(1);
			y2 = gval(2);
			y3 = gval(3);
			
			l_m00 = ((y1-y0)*HERMITE_TENSION) >> 16; // 16.0
			l_m01 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			l_m0	= l_m00 + l_m01;
			
			l_m10 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			l_m11 = ((y3-y2)*HERMITE_TENSION) >> 16; // 16.0
			l_m1	= l_m10 + l_m11;
			
			l_val = ((	2*y1 +	 l_m0 + l_m1 - 2*y2) * mu) >> 16; // 16.0
			l_val = ((l_val - 3*y1 - 2*l_m0 - l_m1 + 3*y2) * mu) >> 16; // 16.0
			l_val = ((l_val 			 +	 l_m0 					 ) * mu) >> 16; // 16.0
			
			fa = l_val + (y1>>0);
		} break;
		//--------------------------------------------------//
	case 4: 																						// cubic interpolation
		{
			long xd;int gpos;
			
			// safety check
			xd = (pChannel->spos >> 1)+1;
			gpos = pChannel->SB[28];
			
			fa	= gval(3) - 3*gval(2) + 3*gval(1) - gval(0);
			fa *= (xd - (2<<15)) / 6;
			fa >>= 15;
			fa += gval(2) - gval(1) - gval(1) + gval(0);
			fa *= (xd - (1<<15)) >> 1;
			fa >>= 15;
			fa += gval(1) - gval(0);
			fa *= xd;
			fa >>= 15;
			fa = fa + gval(0);
			
		} break;
		//--------------------------------------------------//
	 case 3:
		 {
		 /*
		 ADPCM interpolation (4-tap FIR)
		 
			 y[n] = (x[n-3] * 4807 + x[n-2] * 22963 + x[n-1] * 4871 - x[n]) >> 15;
			 
				 - Dr. Hell (Xebra PS1 emu)
			 */
			 
			 int gpos;
			 gpos = pChannel->SB[28];
			 
			 fa = (gval(3) * 4807 + gval(2) * 22963 + gval(1) * 4871 - gval(0)) >> 15;
		 } break;
		 //--------------------------------------------------//
	 case 2:																						 // gauss interpolation
		 {
			 int vl, vr;int gpos;
			 
			 // Mednafen
			 // 0x59b3 / 0x8000 = 0.70079653309732352671895504623554
			 
			 // Neill
			 // 1305/2048 = 0.63720703125
			 
			 // safety check
			 vl = ((pChannel->spos & 0xffff) >> 6) & ~3;
			 gpos = pChannel->SB[28];
			 vr=(gauss[vl]*gval0) >> 15;
			 vr+=(gauss[vl+1]*gval(1)) >> 15;
			 vr+=(gauss[vl+2]*gval(2)) >> 15;
			 vr+=(gauss[vl+3]*gval(3)) >> 15;
			 fa = vr;
		 } break;
		 //--------------------------------------------------//
	 case 1:																						 // simple interpolation
		 {
			 if(pChannel->sinc<0x10000L)											 // -> upsampling?
				 InterpolateUp(pChannel); 										// --> interpolate up
			 else InterpolateDown(pChannel);									 // --> else down
			 fa=pChannel->SB[29];
		 } break;
		 //--------------------------------------------------//
	 default: 																					 // no interpolation
		 {
			 fa=pChannel->SB[29]; 								 
		 } break;
		 //--------------------------------------------------//
	}
	
	return CLAMP16( fa );
}

////////////////////////////////////////////////////////////////////////
// MAIN SPU FUNCTION
// here is the main job handler... thread, timer or direct func call
// basically the whole sound processing is done in this fat func!
////////////////////////////////////////////////////////////////////////

// 5 ms waiting phase, if buffer is full and no new sound has to get started
// .. can be made smaller (smallest val: 1 ms), but bigger waits give
// better performance

#define PAUSE_W 1
#define PAUSE_L 1000

////////////////////////////////////////////////////////////////////////

int iSpuAsyncWait=0;

extern int old_irq;



// Phantom padder
#if 0
VOID CALLBACK AsyncBuffer(UINT nTimerId,UINT msg,DWORD dwUser,DWORD dwParam1, DWORD dwParam2)
{
	// no async card blocker = bye!
	thread_async_ended = 1;

	if( phantom_padder == 0 ) return;
	if( bEndThread == 1 ) return;
	if( iUseTimer <= 1 ) return;
	
	
	// thread safety
	thread_async_ended = 0;
	timeSetEvent(PAUSE_W,1,AsyncBuffer,0,TIME_ONESHOT);
	
	
	// Dekker's algorithm - share single-use resource
	mutex_flag[ MUTEX_PHANTOM ] = 1;
	if( mutex_flag[ MUTEX_SPU ] == 1 ) {
		mutex_flag[ MUTEX_PHANTOM ] = 0;
		return;
	}

	
	
	if( debug_sound_buffer ) {
		if( fp_spu_log == 0 ) {
			fp_spu_log = fopen( "logg.txt", "w" );
		}


		fprintf( fp_spu_log, "Start phantom pad\n" );
	}
	
	
	
	SoundPhantomPad();
	

	mutex_flag[ MUTEX_PHANTOM ] = 0;
}
#endif




#ifdef _WINDOWS
VOID CALLBACK MAINProc(UINT nTimerId,UINT msg,DWORD dwUser,DWORD dwParam1, DWORD dwParam2)
#else
static void *MAINThread(void *arg)
#endif
{
	int s_1,s_2,fa,ns,voldiv=iVolume;
	unsigned char * start;unsigned int nSample;
	int ch,predict_nr,shift_factor,flags,d,s;
	int bIRQReturn=0;SPUCHAN * pChannel;
	int decoded_voice;
	
	
	while(!bEndThread)																		// until we are shutting down
	{
		//--------------------------------------------------//
		// ok, at the beginning we are looking if there is
		// enuff free place in the dsound/oss buffer to
		// fill in new data, or if there is a new channel to start.
		// if not, we wait (thread) or return (timer/spuasync)
		// until enuff free place is available/a new channel gets
		// started
		
		if( iUseTimer <= 2 )
		{
			if(dwNewChannel)																		// new channel should start immedately?
			{ 																								 // (at least one bit 0 ... MAXCHANNEL is set?)
				iSecureStart++; 																	// -> set iSecure
				if(iSecureStart>5) iSecureStart=0;								//		(if it is set 5 times - that means on 5 tries a new samples has been started - in a row, we will reset it, to give the sound update a chance)
			}
			else iSecureStart=0;																// 0: no new channel should start
			
			
			while(!iSecureStart && !bEndThread && 							// no new start? no thread end?
				// and still enuff data in sound buffer?
				(SoundGetBytesBuffered() > SOUNDLEN(5+LATENCY)) )
			{
				iSecureStart=0; 																	// reset secure
				
#ifdef _WINDOWS
				if(iUseTimer) 																		// no-thread mode?
				{
					if(iUseTimer==1)																// -> ok, timer mode 1: setup a oneshot timer of x ms to wait
						timeSetEvent(PAUSE_W,1,MAINProc,0,TIME_ONESHOT);
					return; 																				// -> and done this time (timer mode 1 or 2)
				}
				// win thread mode:
				Sleep(PAUSE_W); 																	// sleep for x ms (win)
#else
				if(iUseTimer) return 0; 													// linux no-thread mode? bye
				Sleep(PAUSE_W); 																	// else sleep for x ms (linux)
#endif
				
				if(dwNewChannel) iSecureStart=1;									// if a new channel kicks in (or, of course, sound buffer runs low), we will leave the loop
			}
		}
		//--------------------------------------------------// continue from irq handling in timer mode? 
		
		if(lastch>=0) 																			// will be -1 if no continue is pending
		{
			ch=lastch; ns=lastns; lastch=-1;									// -> setup all kind of vars to continue
			pChannel=&s_chan[ch];
			goto GOON;																				// -> directly jump to the continue point
		}
		
		//--------------------------------------------------//
		//- main channel loop 														 -// 
		//--------------------------------------------------//
		{
			ns=0;
			decoded_voice = decoded_ptr;
			
			InitREVERB();
			
			
			while(ns<APU_run) 															 // loop until 1 ms of data is reached
			{
				SSumL[ns]=0;
				SSumR[ns]=0;
				
				
				// decoded buffer values - dummy
				spuMem[ (0x000 + decoded_voice) / 2 ] = (short) 0;
				spuMem[ (0x400 + decoded_voice) / 2 ] = (short) 0;
				spuMem[ (0x800 + decoded_voice) / 2 ] = (short) 0;
				spuMem[ (0xc00 + decoded_voice) / 2 ] = (short) 0;
				
				
				
				NoiseClock();
				
				pChannel=s_chan;
				for(ch=0;ch<MAXCHAN;ch++,pChannel++)							// loop em all... we will collect 1 ms of sound of each playing channel
				{
					if(pChannel->bNew) 
					{
						if( pChannel->ADSRX.StartDelay == 0 ) {
							StartSound(pChannel); 												// start new sound
							dwNewChannel&=~(1<<ch); 											// clear new channel bit
						} else {
							pChannel->ADSRX.StartDelay--;
						}
					}
					
					
					if(!pChannel->bOn) continue;										// channel not playing? next
					
					
					// BIOS - uses $1000
					// - decoded voice = off area (???)
					
					if( pChannel->pCurr - spuMemC < 0x1000 ) {
						pChannel->bOn = 0;
						
						pChannel->iSilent=2;
						
						pChannel->ADSRX.lVolume=0;
						pChannel->ADSRX.EnvelopeVol=0;
						pChannel->ADSRX.EnvelopeVol_f=0;
						
						continue;
					}
					
					
					// Silhouette Mirage - ending mini-game
					// ?? - behavior?
					
					if( pChannel->pCurr - spuMemC >= 0x80000 ) {
						// dead channel - abort (no more IRQs)
						pChannel->bOn = 0;
						
						pChannel->iSilent = 2;
						
						pChannel->ADSRX.lVolume=0;
						pChannel->ADSRX.EnvelopeVol=0;
						pChannel->ADSRX.EnvelopeVol_f=0;
						
						continue;
					}
					
					
					
					if(pChannel->iActFreq!=pChannel->iUsedFreq) 		// new psx frequency?
						VoiceChangeFrequency(pChannel);
					
					if(pChannel->bFMod==1 && iFMod[ns]) 					// fmod freq channel
						FModChangeFrequency(pChannel,ns);
					
					while(pChannel->spos>=0x10000L)
					{
						if(pChannel->iSBPos==28)										// 28 reached?
						{
						/*
						Xenogears - must do $4 flag here ($7 sound effects)
						Jungle Book - external loop
						Xenogears - Anima Relic dungeons
							*/
							if( pChannel->bLoopJump == 1 ) {
								pChannel->pCurr = pChannel->pLoop;
								
								
								// ??? - stop illegal addresses
								if( pChannel->pCurr - spuMemC < 0x1000 ) {
									pChannel->bOn = 0;
									
									
									pChannel->iSilent=2;
									
									pChannel->ADSRX.lVolume=0;
									pChannel->ADSRX.EnvelopeVol=0;
									pChannel->ADSRX.EnvelopeVol_f=0;
									
									
									// abort - don't trigger IRQs
									break;
								}
								
								
								/*
								Metal Gear Solid
								- Does an on-off clear test at start
								- ???: stop playback to avoid dupe IRQ @ ch 1
								*/
								
								if( pChannel->pCurr - spuMemC == 0x1000 &&
									pChannel->iSilent == 2 ) {
									pChannel->bOn = 0;
									
									pChannel->ADSRX.lVolume=0;
									pChannel->ADSRX.EnvelopeVol=0;
									pChannel->ADSRX.EnvelopeVol_f=0;
									
									break;
								}
								
								
								// Nuclear Strike / Soviet Strike
								if( Check_IRQ( (pChannel->pCurr-spuMemC)-0, 0 ) )
								{
#ifdef SPU_LOG
									fprintf( fp_spu_log, "%d = IRQ %X\n", ch+1, pSpuIrq-spuMemC );
#endif
									
									pChannel->iIrqDone=1; 								// -> debug flag
									
									if(iSPUIRQWait) 											// -> option: wait after irq for main emu
									{
										iSpuAsyncWait=1;
										bIRQReturn=1;
									}
								}
							}
							
							pChannel->bLoopJump = 0;
							
							
							
							start=pChannel->pCurr;										// set up the current pos
							
							if (pChannel->iSilent==1 || start == (unsigned char*)-1)					// special "stop" sign
							{
								// silence = let channel keep running (IRQs)
								//pChannel->bOn=0;												// -> turn everything off
								pChannel->iSilent=2;
								
								// Actua Soccer 2 - stop envelope now
								pChannel->ADSRX.lVolume=0;
								pChannel->ADSRX.EnvelopeVol=0;
								pChannel->ADSRX.EnvelopeVol_f=0;
							}
							
							pChannel->iSBPos=0;
							
							//////////////////////////////////////////// spu irq handler here? mmm... do it later
							
							s_1=pChannel->s_1;
							s_2=pChannel->s_2;
							
							predict_nr=(int)*start;start++;
							shift_factor=predict_nr&0xf;
							predict_nr >>= 4;
							flags=(int)*start;start++;
							
							
							// Silhouette Mirage - Serah fight
							if( predict_nr > 4 ) predict_nr = 0;
							
							// -------------------------------------- // 
							
							for (nSample=0;nSample<28;start++)			
							{
								int t1,t2;
								
								
								// OPTIMIZE - skip this
								if( pChannel->iSilent == 2 ) {
									// don't use break - start++ keeps track of this
									nSample += 2;
									continue;
								}
								
								
								d=(int)*start;
								s=((d&0xf)<<12);
								if(s&0x8000) s|=0xffff0000;
								
								// -------------------------------
								
								fa= (s >> shift_factor);
								
								t1 = (s_1 * f[predict_nr][0])/64;
								t2 = (s_2 * f[predict_nr][1])/64;
								
								// MTV Music Generator - don't clamp here (demo1 vocal)
								//CLAMP16(t1); CLAMP16(t2);
								//fa + CLAMP16(t1+t2)/64
								
								// snes brr clamps
								fa = CLAMP16( fa + (t1 + t2) );
								
								s_2=s_1;s_1=fa;
								pChannel->SB[nSample++]=fa;
								
								
								
								s=((d & 0xf0) << 8);
								if(s&0x8000) s|=0xffff0000;
								
								fa= (s>>shift_factor);
								
								t1 = (s_1 * f[predict_nr][0])/64;
								t2 = (s_2 * f[predict_nr][1])/64;
								
								// MTV Music Generator - don't clamp here (demo1 vocal)
								//CLAMP16(t1); CLAMP16(t2);
								//fa + CLAMP16(t1+t2)/64
								
								// snes brr clamps
								fa = CLAMP16( fa + (t1 + t2) );
								
								s_2=s_1;s_1=fa;
								pChannel->SB[nSample++]=fa;
							} 		
							
							//////////////////////////////////////////// irq check
							
							// Misadventures of Tron Bonne uses (-8)
							if( Check_IRQ( (start-spuMemC)-8, 0 ) ||
								Check_IRQ( (start-spuMemC)-0, 0 ) )
							{
#ifdef SPU_LOG
								fprintf( fp_spu_log, "%d = IRQ %X\n", ch+1, pSpuIrq-spuMemC );
#endif
								
								pChannel->iIrqDone=1; 								// -> debug flag
								
								if(iSPUIRQWait) 											// -> option: wait after irq for main emu
								{
									iSpuAsyncWait=1;
									bIRQReturn=1;
								}
							}
							
							//////////////////////////////////////////// flag handler
							
							/*
							SPU2-X (PCSX2 team):
							$4 = set loop to current block
							$2 = keep envelope on (no mute)
							$1 = jump to loop address
							
								silence means no volume (ADSR keeps playing!!)
							*/
							
							
							// Misadventures of Tron Bonne
							// - ignore illegal flags
							if( flags > 7 ) {
								flags = 0;
								
								
								// Tron Bonne (???)
								// - Final boss with Loath (set envelope -next- loop)
								pChannel->iSilent = 1;
							}
							
							
							
							// Xenogears - must do $4 flag here (sound effects)
							if(flags&4) {
								// Xenogears - Anima Relic dungeons
								pChannel->pLoop = start-16;
							}
							
							
							// Jungle Book - don't reset (wrong gameplay speed - IRQ hits)
							//pChannel->bIgnoreLoop = 0;
							
							
							if(flags&1)
							{
								// set jump flag
								pChannel->bLoopJump = 1;
								
								// Xenogears - 7 = menu sound + other missing sounds
								//start = pChannel->pLoop;
								
								// silence = keep playing
								if( (flags&2) == 0 ) {
									// silence = don't start release phase
									//pChannel->bStop = 1;
									
									// Xenogears - shutdown volume
									// 1 - right now
									// 2 - right after block plays (*)
									// - fixes cavern water drops
									pChannel->iSilent = 1;
								}
								else {
									// Jungle Book - don't set silent back to off (loop buzz)
									//pChannel->iSilent = 0;
								}
								
								
								// Jungle Book - don't do this (scratchy)
								//s_1 = 0;
								//s_2 = 0;
							}
							
							
#if 0
							// crash check
							if( start == 0 )
								start = (unsigned char *) -1;
							if( start >= spuMemC + 0x80000 )
								start = spuMemC - 0x80000;
#endif
							
							
							pChannel->pCurr=start;										// store values for next cycle
							pChannel->s_1=s_1;
							pChannel->s_2=s_2;			
							
							////////////////////////////////////////////
							
							if( iUseTimer <= 2 && bIRQReturn)														// special return for "spu irq - wait for cpu action"
							{
								bIRQReturn=0;
								if(iUseTimer<2)
								{ 
									DWORD dwWatchTime=timeGetTime()+2500;
									
									while(iSpuAsyncWait && !bEndThread && 
										timeGetTime()<dwWatchTime)
#ifdef _WINDOWS
										Sleep(1);
#else
									Sleep(1); 
#endif
									
								}
								else
								{
									lastch=ch; 
									lastns=ns;
									
#ifdef _WINDOWS
									return;
#else
									return 0;
#endif
								}
							}
							
							////////////////////////////////////////////
							
GOON: ;
						}
						
						fa = pChannel->SB[pChannel->iSBPos++];				// get sample data
						
						StoreInterpolationVal(pChannel,fa); 				// store val for later interpolation
						
						pChannel->spos -= 0x10000L;
					}
					
					////////////////////////////////////////////////
					
					// OPTIMIZE - skip this
					// - Tron Bonne hack
					if( pChannel->iSilent == 2 )
						fa = 0;
					
					// get noise val
					else if(pChannel->bNoise)
						fa= ( iGetNoiseVal(pChannel) );
					
					// get sample val
					else
						fa= ( iGetInterpolationVal(pChannel) );
					
					
					
					// Voice 1/3 decoded buffer
					if( ch == 0 ) {
						spuMem[ (0x800 + decoded_voice) / 2 ] = (short) fa;
					} else if( ch == 2 ) {
						spuMem[ (0xc00 + decoded_voice) / 2 ] = (short) fa;
					}
					
					
					spu_ch = ch;
					
					// Actua Soccer 2 - stop envelope
					if( pChannel->iSilent == 2 )
						pChannel->sval = 0;
					else
						// assume 15-bit value + sign-bit
						pChannel->sval= ( (MixADSR(pChannel)*fa)/0x8000 );	 // mix adsr
					
					
					if(pChannel->bFMod==2)												// fmod freq channel
						iFMod[ns]=pChannel->sval; 									 // -> store 1T sample data, use that to do fmod on next channel
					
					
					// Xenogears: mix fmod channel into output
					// - fixes save icon (high pitch)
					if( pChannel->sval != 0 )
					{
						//////////////////////////////////////////////
						// ok, left/right sound volume (psx volume goes from 0 ... 0x3fff)
						
						// OPTIMIZE
						if(pChannel->iMute) 
							pChannel->sval=0; 												// debug mute
						else
						{
							int lc,rc;
							
							// assume 14-bit value + sign
							lc = ( (pChannel->sval * pChannel->iLeftVolume) / 0x4000L );
							rc = ( (pChannel->sval * pChannel->iRightVolume) / 0x4000L );
							
							SSumL[ns] = ( SSumL[ns] + lc );
							SSumR[ns] = ( SSumR[ns] + rc );
							
							
							//////////////////////////////////////////////
							// now let us store sound data for reverb 	 
							
							// check reverb write flags
							if( pChannel->bRVBActive )
								StoreREVERB(pChannel,ns);
						}
					}
					
					pChannel->spos += pChannel->sinc;
				} // end ch
				
				
				////////////////////////////////////////////////
				// ok, go on until 1 ms data of this channel is collected
				
				// voice boost
				SSumL[ns] = ( (SSumL[ns] * iVolVoices) / 10 );
				SSumR[ns] = ( (SSumR[ns] * iVolVoices) / 10 );
				
				
				// decoded buffer - voice
				decoded_voice += 2;
				decoded_voice &= 0x3ff;
				
				
				// status flag
				if( decoded_voice >= 0x200 ) {
					spuStat |= STAT_DECODED;
				} else {
					spuStat &= ~STAT_DECODED;
				}
				
				
				// IRQ work
				// - OPTIMIZE
				if( (spuCtrl & CTRL_IRQ) && (pSpuIrq - spuMemC < 0x1000) )
				{
					// check all decoded buffer IRQs - timing issue
					Check_IRQ( decoded_voice + 0x000, 0 );
					Check_IRQ( decoded_voice + 0x400, 0 );
					Check_IRQ( decoded_voice + 0x800, 0 );
					Check_IRQ( decoded_voice + 0xc00, 0 );
				}
				
				
				ns++;
			} // end ns
		} // end main channel code
		
		//---------------------------------------------------//
		//- here we have another 1 ms of sound data
		//---------------------------------------------------//
		// mix XA infos (if any)
		
		if(XAPlay!=XAFeed || XARepeat) MixXA();
		if(CDDAPlay!=CDDAFeed || CDDARepeat) MixCDDA();
		
		
		// now safe to update deocded buffer ptr
		decoded_ptr += ns * 2;
		decoded_ptr &= 0x3ff;
		
		
		///////////////////////////////////////////////////////
		// mix all channels (including reverb) into one buffer
		
		for(ns=0;ns<APU_run;ns++)
		{ 					 
			int lc,rc;
			int voll,volr;
			
			
			lc = CLAMP16( SSumL[ns] + MixREVERBLeft(ns) );
			rc = CLAMP16( SSumR[ns] + MixREVERBRight() );
			
			
			// Die Hard 1 fade-in ($4000)
			if( iVolMainL & 0x4000 ) voll = (iVolMainL & 0x3fff) - 0x4000;
			else voll = iVolMainL & 0x3fff;
			
			if( iVolMainR & 0x4000 ) volr = (iVolMainR & 0x3fff) - 0x4000;
			else volr = iVolMainR & 0x3fff;
			
			
			lc = CLAMP16( (lc * voll) / 0x4000 );
			rc = CLAMP16( (rc * volr) / 0x4000 );
			
			
			OutStoreInterpolationVal( lc, rc );
			OutGetInterpolationVal(
				(0x10000 * iOutput2Strength) / 10, (0x10000 * iOutput2Strength) / 10,
				&lc, &rc );
			
			
			
			if(iOutputInterp1)
			{
				double ldiff, rdiff, avg, tmp;
				static double _interpolation_coefficient = 3.759285613;
				
				
				/*
				Frequency Response
				- William Pitcock (nenolod) (UPSE PSF player)
				- http://nenolod.net
				*/
				
				avg = ((lc + rc) / 2);
				ldiff = lc - avg;
				rdiff = rc - avg;
				
				tmp = avg + ldiff * _interpolation_coefficient;
				lc = CLAMP16( (int) tmp );
				
				tmp = avg + rdiff * _interpolation_coefficient;
				rc = CLAMP16( (int) tmp );
			}
			
			
			lc = CLAMP16( (lc*voldiv)/10 ); SSumL[ns]=0;
			rc = CLAMP16( (rc*voldiv)/10 ); SSumR[ns]=0;
			
			
			// stereo -> mono
			if(iDisStereo) {
				int temp;
				
				temp = (lc + rc) / 2;
				
				lc = temp;
				rc = temp;
			}
			
			
			// speaker setup
			switch( output_channels ) {
				// mono
			case 1:
				*pS++ = lc;
				break;
				
				// stereo - left + right
			case 2:
				*pS++ = lc;
				*pS++ = rc;
				break;
				
				
				// SPU2-X upmixer (3+ speakers)
				// - pcsx2.net
				
				// 2.1 - left + right + LFE
			case 3:
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = ( lc + rc ) / 2;
				break;
				
				// Quad - left + right + leftback + rightback
			case 4:
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = lc;
				*pS++ = rc;
				break;
				
				// 4.1 - left + right + LFE + leftback + rightback
			case 5:
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = lc;
				*pS++ = rc;
				break;
				
				// 5.1 - left + right + center + LFE + leftback + rightback
			case 6:
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = lc;
				*pS++ = rc;
				break;
				
				// 6.1 - left + right + center + LFE + leftback + rightback + backcenter
			case 7:
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = ( lc + rc ) / 2;
				break;
				
				// 7.1 - left + right + center + LFE + leftback + rightback + leftside + rightside
			case 8:
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = lc;
				*pS++ = rc;
				*pS++ = ( lc + rc ) / 2;
				*pS++ = ( lc + rc ) / 2;
				break;
				
			case 9:
				// invalid - needs mapping array
				break;
			}
		}
		
		
		/*
		44100 -> 45xxx
		- output drain too fast
		- interpolation stall
		*/
		
		if( sound_stretcher )
		{
			static int drain = 0;
			int size, stretch;
			
			// speed boost
			size = SoundGetBytesBuffered();
			if( size == 0 ) size = SOUNDLEN(LATENCY);
			

			size -= SOUNDLEN(18);

			
			drain++;
			stretch = 0;
			
			/*
			slower emu parts drain faster
			- best way to average different speeds
			
				- MML2 = (50-75 avg at peaks)
				- Xenogears = ~50-75 at peaks
				- Tekken 3 = even slower?
				- control descent from 120
			*/
			
			switch( latency_target / 10 ) {
			case 1:
				if(
					(size < SOUNDLEN(5) && drain >= 30) ||
					(size < SOUNDLEN(10) && drain >= 50) ||
					(size < SOUNDLEN(15) && drain >= 70) ||
					(size < SOUNDLEN(20) && drain >= 100) ||
					0
					)
				{
					stretch = 1;
				}
				break;
				
				
			case 2:
				if(
					(size < SOUNDLEN(10) && drain >= 30) ||
					(size < SOUNDLEN(15) && drain >= 50) ||
					(size < SOUNDLEN(20) && drain >= 80) ||
					(size < SOUNDLEN(25) && drain >= 200) ||
					(size < SOUNDLEN(30) && drain >= 400) ||
					0
					)
				{
					stretch = 1;
				}
				break;
				
				
			case 3:
				if(
					(size < SOUNDLEN(10) && drain >= 30) ||
					(size < SOUNDLEN(15) && drain >= 40) ||
					(size < SOUNDLEN(20) && drain >= 50) ||
					(size < SOUNDLEN(25) && drain >= 80) ||
					(size < SOUNDLEN(30) && drain >= 200) ||
					(size < SOUNDLEN(35) && drain >= 400) ||
					(size < SOUNDLEN(40) && drain >= 1100) ||
					0
					)
				{
					stretch = 1;
				}
				break;
				
				
			case 4:
				if(
					(size < SOUNDLEN(10) && drain >= 20) ||
					(size < SOUNDLEN(15) && drain >= 30) ||
					(size < SOUNDLEN(20) && drain >= 40) ||
					(size < SOUNDLEN(25) && drain >= 50) ||
					(size < SOUNDLEN(30) && drain >= 70) ||
					(size < SOUNDLEN(35) && drain >= 100) ||
					(size < SOUNDLEN(40) && drain >= 300) ||
					(size < SOUNDLEN(45) && drain >= 500) ||
					(size < SOUNDLEN(50) && drain >= 1100) ||
					0
					)
				{
					stretch = 1;
				}
				break;
				
				
			case 5:
				if(
					(size < SOUNDLEN(10) && drain >= 30) ||
					(size < SOUNDLEN(15) && drain >= 40) ||
					(size < SOUNDLEN(20) && drain >= 50) ||
					(size < SOUNDLEN(25) && drain >= 60) ||
					(size < SOUNDLEN(30) && drain >= 70) ||
					(size < SOUNDLEN(35) && drain >= 80) ||
					(size < SOUNDLEN(40) && drain >= 90) ||
					(size < SOUNDLEN(45) && drain >= 100) ||
					(size < SOUNDLEN(50) && drain >= 300) ||
					(size < SOUNDLEN(55) && drain >= 500) ||
					(size < SOUNDLEN(60) && drain >= 1100) ||
					0
					)
				{
					stretch = 1;
				}
				break;
				
				
			case 6:
				if(
					(size < SOUNDLEN(20) && drain >= 30) ||
					(size < SOUNDLEN(25) && drain >= 40) ||
					(size < SOUNDLEN(30) && drain >= 50) ||
					(size < SOUNDLEN(35) && drain >= 60) ||
					(size < SOUNDLEN(40) && drain >= 70) ||
					(size < SOUNDLEN(45) && drain >= 80) ||
					(size < SOUNDLEN(50) && drain >= 90) ||
					(size < SOUNDLEN(55) && drain >= 100) ||
					(size < SOUNDLEN(60) && drain >= 300) ||
					(size < SOUNDLEN(65) && drain >= 500) ||
					(size < SOUNDLEN(70) && drain >= 1100) ||
					0
					)
				{
					stretch = 1;
				}
				break;
				
				
			case 7:
				if(
					(size < SOUNDLEN(30) && drain >= 30) ||
					(size < SOUNDLEN(35) && drain >= 40) ||
					(size < SOUNDLEN(40) && drain >= 50) ||
					(size < SOUNDLEN(45) && drain >= 60) ||
					(size < SOUNDLEN(50) && drain >= 70) ||
					(size < SOUNDLEN(55) && drain >= 80) ||
					(size < SOUNDLEN(60) && drain >= 90) ||
					(size < SOUNDLEN(65) && drain >= 100) ||
					(size < SOUNDLEN(70) && drain >= 300) ||
					(size < SOUNDLEN(75) && drain >= 500) ||
					(size < SOUNDLEN(80) && drain >= 1100) ||
					0
					)
				{
					stretch = 1;
				}
				break;
				
				
			case 8:
				if(
					(size < SOUNDLEN(40) && drain >= 30) ||
					(size < SOUNDLEN(45) && drain >= 40) ||
					(size < SOUNDLEN(50) && drain >= 50) ||
					(size < SOUNDLEN(55) && drain >= 60) ||
					(size < SOUNDLEN(60) && drain >= 70) ||
					(size < SOUNDLEN(65) && drain >= 80) ||
					(size < SOUNDLEN(70) && drain >= 90) ||
					(size < SOUNDLEN(75) && drain >= 100) ||
					(size < SOUNDLEN(80) && drain >= 300) ||
					(size < SOUNDLEN(85) && drain >= 500) ||
					(size < SOUNDLEN(90) && drain >= 1100) ||
					0
					)
				{
					stretch = 1;
				}
				break;
			} // end latency checks
			
			
			if( stretch ) {
				drain = 0;
				
				switch( output_channels ) {
					
					// mono
				case 0:
					*(pS+0) = *(pS-1);
					pS++;
					break;
					
					// stereo
				case 2:
					*(pS+0) = *(pS-2);
					*(pS+1) = *(pS-1);
					
					pS++;
					pS++;
					break;
					
					// 2.1
				case 3:
					*(pS+0) = *(pS-3);
					*(pS+1) = *(pS-2);
					*(pS+2) = *(pS-1);
					
					pS++;
					pS++;
					pS++;
					break;
					
					// quad
				case 4:
					*(pS+0) = *(pS-4);
					*(pS+1) = *(pS-3);
					*(pS+2) = *(pS-2);
					*(pS+3) = *(pS-1);
					
					pS++;
					pS++;
					pS++;
					pS++;
					break;
					
					// 4.1
				case 5:
					*(pS+0) = *(pS-5);
					*(pS+1) = *(pS-4);
					*(pS+2) = *(pS-3);
					*(pS+3) = *(pS-2);
					*(pS+4) = *(pS-1);
					
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					break;
					
					// 5.1
				case 6:
					*(pS+0) = *(pS-6);
					*(pS+1) = *(pS-5);
					*(pS+2) = *(pS-4);
					*(pS+3) = *(pS-3);
					*(pS+4) = *(pS-2);
					*(pS+5) = *(pS-1);
					
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					break;
					
					// 6.1
				case 7:
					*(pS+0) = *(pS-7);
					*(pS+1) = *(pS-6);
					*(pS+2) = *(pS-5);
					*(pS+3) = *(pS-4);
					*(pS+4) = *(pS-3);
					*(pS+5) = *(pS-2);
					*(pS+6) = *(pS-1);
					
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					break;
					
					// 7.1
				case 8:
					*(pS+0) = *(pS-8);
					*(pS+1) = *(pS-7);
					*(pS+2) = *(pS-6);
					*(pS+3) = *(pS-5);
					*(pS+4) = *(pS-4);
					*(pS+5) = *(pS-3);
					*(pS+6) = *(pS-2);
					*(pS+7) = *(pS-1);
					
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					pS++;
					break;
				}
			}
		} // end async wait checker
		
		
		//////////////////////////////////////////////////////									 
		// special irq handling in the decode buffers (0x0000-0x1000)
		// we know: 
		// the decode buffers are located in spu memory in the following way:
		// 0x0000-0x03ff	CD audio left
		// 0x0400-0x07ff	CD audio right
		// 0x0800-0x0bff	Voice 1
		// 0x0c00-0x0fff	Voice 3
		// and decoded data is 16 bit for one sample
		// we assume: 
		// even if voices 1/3 are off or no cd audio is playing, the internal
		// play positions will move on and wrap after 0x400 bytes.
		// Therefore: we just need a pointer from spumem+0 to spumem+3ff, and 
		// increase this pointer on each sample by 2 bytes. If this pointer
		// (or 0x400 offsets of this pointer) hits the spuirq address, we generate
		// an IRQ. Only problem: the "wait for cpu" option is kinda hard to do here
		// in some of Peops timer modes. So: we ignore this option here (for now).
		// Also note: we abuse the channel 0-3 irq debug display for those irqs
		// (since that's the easiest way to display such irqs in debug mode :))
		
		
		////////////////////////////////////////////////////// 									
		// feed the sound
		// latency = ms
		
		iCycle += (APU_run * output_samplesize);
		
		if(iCycle >= UPLOADSIZE)
		{
			int test;
			
			//- zn qsound mixer callback ----------------------//
			
			if(irqQSound)
			{
				long * pl=(long *)XAPlay;
				short * ps=(short *)pSpuBuffer;
				int g,iBytes=((unsigned char *)pS)-((unsigned char *)pSpuBuffer);
				
				//memcpy( pl, ps, iBytes );
				iBytes/=2;
				for(g=0;g<iBytes;g++) {*pl++=*ps++;}
				
				irqQSound((unsigned char *)pSpuBuffer,(long *)XAPlay,iBytes/2);
			}
			

			if( phantom_padder ) {
				// Dekker's algorithm - share single-use resource
				mutex_flag[ MUTEX_SPU ] = 1;
				while( mutex_flag[ MUTEX_PHANTOM ] == 1 ) {}
			}

			
			test = SoundGetBytesBuffered();
				
				
			if( debug_sound_buffer ) {
				extern FILE *fp_spu_log;
					
				if( fp_spu_log == 0 ) {
					fp_spu_log = fopen( "logg.txt", "w" );
				}
					
				fprintf( fp_spu_log, "(stream b) %d %d || %d\n", test, LastWrite,
					((unsigned char *)pS)-((unsigned char *)pSpuBuffer) );
			}
				
				
			if( framelimiter == 1 )
			{
				if( iUseTimer == 3 ) {
					while( test > SOUNDLEN( LATENCY + async_wait_block ) ) {
						Sleep(1);
							
						test = SoundGetBytesBuffered();
					}
				}
					
				else if( iUseTimer == 4 ) {
					while( test > SOUNDLEN( LATENCY + async_ondemand_block ) ) {
						test = SoundGetBytesBuffered();
					}
				}
			}
				
				
			if( debug_sound_buffer ) {
				extern FILE *fp_spu_log;
					
				if( fp_spu_log == 0 ) {
					fp_spu_log = fopen( "logg.txt", "w" );
				}
					
				fprintf( fp_spu_log, "(stream w) %d %d || %d\n", test, LastWrite,
					((unsigned char *)pS)-((unsigned char *)pSpuBuffer) );
			}
				
				
			// overflow check - likely fast-forward
			if( test < TESTMAX )
				SoundFeedStreamData((unsigned char*)pSpuBuffer,
					((unsigned char *)pS)-
					((unsigned char *)pSpuBuffer));
			else
				SoundRecordStreamData((unsigned char*)pSpuBuffer,
					((unsigned char *)pS)-
					((unsigned char *)pSpuBuffer));
			
			
			pS=(short *)pSpuBuffer;
			iCycle = 0;


			// async multithreading - turn on padding again
			mutex_flag[ MUTEX_SPU ] = 0;
		}
		
		
		if( iUseTimer >= 2 )
			break;
 }
 
 // end of big main loop...
 
 bThreadEnded=1;
 
#ifndef _WINDOWS
 return 0;
#endif
}

////////////////////////////////////////////////////////////////////////
// WINDOWS THREAD... simply calls the timer func and stays forever :)
////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS

DWORD WINAPI MAINThreadEx(LPVOID lpParameter)
{
	MAINProc(0,0,0,0,0);
	return 0;
}

#endif

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// SPU ASYNC... even newer epsxe func
//	1 time every 'cycle' cycles... harhar
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUasync(unsigned long cycle)
{
	cpu_cycles += cycle;
	total_cpu_cycles += cycle;
	
	
	if(iSpuAsyncWait)
	{
		iSpuAsyncWait++;
		if(iSpuAsyncWait<=64) return;
		iSpuAsyncWait=0;
		
		cpu_cycles = cycle;
	}
	
#ifdef _WINDOWS
	if(iDebugMode==2)
	{
		if(IsWindow(hWDebug)) DestroyWindow(hWDebug);
		hWDebug=0;iDebugMode=0;
	}
	if(iRecordMode==2)
	{
		if(IsWindow(hWRecord)) DestroyWindow(hWRecord);
		hWRecord=0;iRecordMode=0;
	}
#endif
	
	if(iUseTimer >= 2)																			// special mode, only used in Linux by this spu (or if you enable the experimental Windows mode)
	{
		if(!bSpuInit) return; 															// -> no init, no call
		
		while( cpu_cycles >= cpu_clock * APU_run / 44100 )
		{
#ifdef _WINDOWS
			MAINProc(0,0,0,0,0);																// -> experimental win mode... not really tested... don't like the drawbacks
#else
			MAINThread(0);																			// -> linux high-compat mode
#endif
			
			cpu_cycles -= cpu_clock * APU_run / 44100;
			
			
#if 0
			total_apu_cycles++;
			if( total_cpu_cycles == 44100 ) {
				// re-sync per 1 sec
				cpu_cycles += (cpu_clock - total_cpu_cycles);
				
				total_cpu_cycles = 0;
				total_apu_cycles = 0;
			}
#endif	
			
			
#ifdef SPU_LOG
					 if( !fp_spu_log ) {
						 fp_spu_log = fopen( "spu-log.txt", "w" );
					 } 
					 fprintf( fp_spu_log, "ASYNC\n" );
#endif
		}
	}
}

////////////////////////////////////////////////////////////////////////
// SPU UPDATE... new epsxe func
//	1 time every 32 hsync lines
//	(312/32)x50 in pal
//	(262/32)x60 in ntsc
////////////////////////////////////////////////////////////////////////

#ifndef _WINDOWS

// since epsxe 1.5.2 (linux) uses SPUupdate, not SPUasync, I will
// leave that func in the linux port, until epsxe linux is using
// the async function as well

void CALLBACK SPUupdate(void)
{
	SPUasync(0);
}

#endif

////////////////////////////////////////////////////////////////////////
// XA AUDIO
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUplayADPCMchannel(xa_decode_t *xap)
{
	if(!iUseXA) 	 return;																// no XA? bye
	if(!xap)			 return;
	if(!xap->freq) return;																// no xa freq ? bye
	
	FeedXA(xap);																					// call main XA feeder
}



// CDDA AUDIO
void CALLBACK SPUplayCDDAchannel(short *pcm, int nbytes)
{
	if (!pcm) 		 return;
	if (nbytes<=0) return;
	
	FeedCDDA((unsigned char *)pcm, nbytes);
}


////////////////////////////////////////////////////////////////////////
// INIT/EXIT STUFF
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// SETUPTIMER: init of certain buffers and threads/timers
////////////////////////////////////////////////////////////////////////

unsigned long CDDABuffer[44100 * 4 * 10];
void SetupTimer(void)
{
	memset(SSumR,0,NSSIZE*sizeof(int)); 									// init some mixing buffers
	memset(SSumL,0,NSSIZE*sizeof(int));
	memset(iFMod,0,NSSIZE*sizeof(int));
	
	pS=(short *)pSpuBuffer; 															// setup soundbuffer pointer
	
	bEndThread=0; 																				// init thread vars
	bThreadEnded=0; 
	bSpuInit=1; 																					// flag: we are inited
	
	
	
	CDDAStart = (unsigned int *) CDDABuffer; 															// alloc cdda buffer
	CDDAEnd 	= (unsigned int *) (CDDAStart + 44100*10);
	CDDAPlay	= (unsigned int *) CDDAStart;
	CDDAFeed	= (unsigned int *) CDDAStart;
	
	
	
#ifdef _WINDOWS
	timeBeginPeriod(1);
	if(iUseTimer==1)																			// windows: use timer
	{
		timeBeginPeriod(1);
		timeSetEvent(1,1,MAINProc,0,TIME_ONESHOT);
	}

	else if( iUseTimer >= 2 ) {
		timeBeginPeriod(1);
		timeSetEvent(PAUSE_W,1,AsyncBuffer,0,TIME_ONESHOT);
	}

	else 
		if(iUseTimer==0)																			// windows: use thread
		{
			//_beginthread(MAINThread,0,NULL);
			DWORD dw;
			hMainThread=CreateThread(NULL,0,MAINThreadEx,0,0,&dw);
			SetThreadPriority(hMainThread,
				//THREAD_PRIORITY_TIME_CRITICAL);
				THREAD_PRIORITY_HIGHEST);
		}
		
#else
		
#ifndef NOTHREADLIB
		if(!iUseTimer)																				// linux: use thread
		{
			pthread_create(&thread, NULL, MAINThread, NULL);
		}
#endif
		
#endif


	// Dekker's algorithm
	mutex_flag[ MUTEX_SPU ] = 0;
	mutex_flag[ MUTEX_PHANTOM ] = 0;
}

////////////////////////////////////////////////////////////////////////
// REMOVETIMER: kill threads/timers
////////////////////////////////////////////////////////////////////////

void RemoveTimer(void)
{
	bEndThread=1; 																				// raise flag to end thread
	
#ifdef _WINDOWS
	
	if(iUseTimer < 2) 																		 // windows thread?
	{
		while(!bThreadEnded) {Sleep(1L);} 									// -> wait till thread has ended
		Sleep(1L);
	}
	if(iUseTimer==1) timeEndPeriod(1);										// windows timer? stop it


	if(iUseTimer >= 2) 																		 // windows thread?
	{
		while(!thread_async_ended) {Sleep(1L);} 						// -> wait till thread has ended
		timeEndPeriod(1);										// windows timer? stop it
	}
#else
	
#ifndef NOTHREADLIB
	if(!iUseTimer)																				// linux tread?
	{
		int i=0;
		while(!bThreadEnded && i<2000) {usleep(1000L);i++;} // -> wait until thread has ended
		if(thread!=-1) {pthread_cancel(thread);thread=-1;}	// -> cancel thread anyway
	}
#endif
	
#endif


	bThreadEnded=0; 																			// no more spu is running
	bSpuInit=0;
}

////////////////////////////////////////////////////////////////////////
// SETUPSTREAMS: init most of the spu buffers
////////////////////////////////////////////////////////////////////////

void SetupStreams(void)
{ 
	int i;
	
	// 16-bit stereo @ 44100 (1 sec)
	pSpuBuffer=(unsigned char *)malloc(44100*8*4);						// alloc mixing buffer
	
	if(iUseReverb==1) i=88200*2;
	else							i=NSSIZE*2;
	
	sRVBStart = (int *)malloc(i*4); 											// alloc reverb buffer
	memset(sRVBStart,0,i*4);
	sRVBEnd  = sRVBStart + i;
	sRVBPlay = sRVBStart;
	
	/*
	Megaman Legends 2
	- async = intro problem
	- output drain too fast / xa drain too slow
	*/
	
	// 16-bit stereo @ 44100*4 (1 sec)
	XAStart = 																						// alloc xa buffer
		(unsigned long *)malloc(44100*4*10);
	XAPlay	= XAStart;
	XAFeed	= XAStart;
	XAEnd 	= XAStart + 44100*10;
}

////////////////////////////////////////////////////////////////////////
// REMOVESTREAMS: free most buffer
////////////////////////////////////////////////////////////////////////

void RemoveStreams(void)
{ 
	free(pSpuBuffer); 																		// free mixing buffer
	pSpuBuffer=NULL;
	free(sRVBStart);																			// free reverb buffer
	sRVBStart=0;
	free(XAStart);																				// free XA buffer
	XAStart=0;
	
	/*
	int i;
	for(i=0;i<MAXCHAN;i++)
	{
	WaitForSingleObject(s_chan[i].hMutex,2000);
	ReleaseMutex(s_chan[i].hMutex);
	if(s_chan[i].hMutex)		
	{CloseHandle(s_chan[i].hMutex);s_chan[i].hMutex=0;}
	}
	*/
}


////////////////////////////////////////////////////////////////////////
// SPUINIT: this func will be called first by the main emu
////////////////////////////////////////////////////////////////////////

long CALLBACK SPUinit(void)
{
	int i;
	
	
	spuMemC=(unsigned char *)spuMem;											// just small setup
	pSpuIrq = spuMemC;
	
	memset((void *)s_chan,0,MAXCHAN*sizeof(SPUCHAN));
	memset((void *)&rvb,0,sizeof(REVERBInfo));
	InitADSR();
	
	
	
	iUseXA=1; 																						// just small setup
	iVolume=10;
	iReverbOff=-1;	 
	spuIrq=0; 											
	spuAddr=0xffffffff;
	bEndThread=0;
	bThreadEnded=0;
	pMixIrq=0;
	memset((void *)s_chan,0,(MAXCHAN+1)*sizeof(SPUCHAN));
	pSpuIrq=spuMemC;
	iSPUIRQWait=0;


	for(i=0;i<MAXCHAN;i++)																// loop sound channels
	{
		// we don't use mutex sync... not needed, would only 
		// slow us down:
		//	 s_chan[i].hMutex=CreateMutex(NULL,FALSE,NULL);
		s_chan[i].ADSRX.SustainLevel = 0xf<<27; 						// -> init sustain
		s_chan[i].iMute=0;
		s_chan[i].iIrqDone=0;
		s_chan[i].pLoop=spuMemC + 0x1000;
		s_chan[i].pStart=spuMemC + 0x1000;
		s_chan[i].pCurr=spuMemC + 0x1000;
	}
	

	iUseDBufIrq = 1;
	if(iUseDBufIrq) pMixIrq=spuMemC;											// enable decoded buffer irqs by setting the address
	
	
	return 0;
}

////////////////////////////////////////////////////////////////////////
// SPUOPEN: called by main emu after init
////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS
long CALLBACK SPUopen(HWND hW)													
#else
long SPUopen(void)
#endif
{
	if(bSPUIsOpen) return 0;															// security for some stupid main emus
	

	ReadConfig(); 																				// read user stuff
	
	
	spuMemC=(unsigned char *)spuMem;			
	
	LastWrite=0xffffffff;LastPlay=0;											// init some play vars
	LastPad=0xffffffff;
	
#ifdef _WINDOWS
	if(!IsWindow(hW)) hW=GetActiveWindow();
	hWMain = hW;																					// store hwnd
#endif
	
	
	SetupStreams(); 																			// prepare streaming
	SetupSound(); 																				// setup sound (before init!)
	SetupTimer(); 																				// timer for feeding data
	
	bSPUIsOpen=1;
	
#ifdef _WINDOWS
	if(iDebugMode)																				// windows debug dialog
	{
		hWDebug=CreateDialog(hInst,MAKEINTRESOURCE(IDD_DEBUG),
			NULL,(DLGPROC)DebugDlgProc);
		SetWindowPos(hWDebug,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOACTIVATE);
		UpdateWindow(hWDebug);
		SetFocus(hWMain);
	}
	
	if(iRecordMode) 																			// windows recording dialog
	{
		hWRecord=CreateDialog(hInst,MAKEINTRESOURCE(IDD_RECORD),
			NULL,(DLGPROC)RecordDlgProc);
		SetWindowPos(hWRecord,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOACTIVATE);
		UpdateWindow(hWRecord);
		SetFocus(hWMain);
	}
#endif
	
	return PSE_SPU_ERR_SUCCESS; 			 
}

////////////////////////////////////////////////////////////////////////

#ifndef _WINDOWS
void SPUsetConfigFile(char * pCfg)
{
	pConfigFile=pCfg;
}
#endif

////////////////////////////////////////////////////////////////////////
// SPUCLOSE: called before shutdown
////////////////////////////////////////////////////////////////////////

long CALLBACK SPUclose(void)
{
	if(!bSPUIsOpen) return 0; 														// some security
	
	bSPUIsOpen=0; 																				// no more open
	
#ifdef _WINDOWS
	if(IsWindow(hWDebug)) DestroyWindow(hWDebug);
	hWDebug=0;
	if(IsWindow(hWRecord)) DestroyWindow(hWRecord);
	hWRecord=0;
#endif
	
	RemoveTimer();																				// no more feeding
	RemoveSound();																				// no more sound handling
	RemoveStreams();																			// no more streaming
	
	return 0;
}

////////////////////////////////////////////////////////////////////////
// SPUSHUTDOWN: called by main emu on final exit
////////////////////////////////////////////////////////////////////////

long CALLBACK SPUshutdown(void)
{
	SPUclose();
	
	return 0;
}

////////////////////////////////////////////////////////////////////////
// SPUTEST: we don't test, we are always fine ;)
////////////////////////////////////////////////////////////////////////

long CALLBACK SPUtest(void)
{
	return 0;
}

////////////////////////////////////////////////////////////////////////
// SPUCONFIGURE: call config dialog
////////////////////////////////////////////////////////////////////////

long CALLBACK SPUconfigure(void)
{
#ifdef _WINDOWS
	DialogBox(hInst,MAKEINTRESOURCE(IDD_CFGDLG),
		GetActiveWindow(),(DLGPROC)DSoundDlgProc);
#else

#endif
	return 0;
}

////////////////////////////////////////////////////////////////////////
// SPUABOUT: show about window
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUabout(void)
{

}

////////////////////////////////////////////////////////////////////////
// SETUP CALLBACKS
// this functions will be called once, 
// passes a callback that should be called on SPU-IRQ/cdda volume change
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUregisterCallback(void (CALLBACK *callback)(void))
{
	irqCallback = callback;
}

void CALLBACK SPUregisterCDDAVolume(void (CALLBACK *CDDAVcallback)(unsigned short,unsigned short))
{
	cddavCallback = CDDAVcallback;
}

////////////////////////////////////////////////////////////////////////
// COMMON PLUGIN INFO FUNCS
////////////////////////////////////////////////////////////////////////

char * CALLBACK PSEgetLibName(void)
{
	return libraryName;
}

unsigned long CALLBACK PSEgetLibType(void)
{
	return	PSE_LT_SPU;
}

unsigned long CALLBACK PSEgetLibVersion(void)
{
	return version<<16|revision<<8|build;
}

char * SPUgetLibInfos(void)
{
	return libraryInfo;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void CALLBACK SPUsetframelimit( int option )
{
	framelimiter = option;
}
