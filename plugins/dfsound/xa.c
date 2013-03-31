/***************************************************************************
                            xa.c  -  description
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

#include "stdafx.h"

#define _IN_XA
#include <stdint.h>

// will be included from spu.c
#ifdef _IN_SPU

////////////////////////////////////////////////////////////////////////
// XA GLOBALS
////////////////////////////////////////////////////////////////////////

xa_decode_t   * xapGlobal=0;

uint32_t * XAFeed  = NULL;
uint32_t * XAPlay  = NULL;
uint32_t * XAStart = NULL;
uint32_t * XAEnd   = NULL;

uint32_t   XARepeat  = 0;
uint32_t   XALastVal = 0;

uint32_t * CDDAFeed  = NULL;
uint32_t * CDDAPlay  = NULL;
uint32_t * CDDAStart = NULL;
uint32_t * CDDAEnd   = NULL;

int             iLeftXAVol  = 0x8000;
int             iRightXAVol = 0x8000;

static int gauss_ptr = 0;
static int gauss_window[8] = {0, 0, 0, 0, 0, 0, 0, 0};

#define gvall0 gauss_window[gauss_ptr]
#define gvall(x) gauss_window[(gauss_ptr+x)&3]
#define gvalr0 gauss_window[4+gauss_ptr]
#define gvalr(x) gauss_window[4+((gauss_ptr+x)&3)]

long cdxa_dbuf_ptr;

////////////////////////////////////////////////////////////////////////
// MIX XA & CDDA
////////////////////////////////////////////////////////////////////////

/*
Attenuation
- Blade_Arma (edgbla) (PCSX-reloaded)
- accurate (!)


s32 lc = (spsound[i ] * attenuators.val0 + spsound[i+1] * attenuators.val3]) / 128;
s32 rc = (spsound[i+1] * attenuators.val2 + spsound[i ] * attenuators.val1]) / 128;
*/

static int lastxa_lc, lastxa_rc;
static int lastcd_lc, lastcd_rc;

INLINE void MixXA(void)
{
 int ns;
 unsigned char val0,val1,val2,val3;
 short l,r;
 int lc,rc;
 unsigned long cdda_l;

 val0 = (iLeftXAVol>>8)&0xff;
 val1 = iLeftXAVol&0xff;
 val2 = (iRightXAVol>>8)&0xff;
 val3 = iRightXAVol&0xff;

 lc = 0;
 rc = 0;

 for(ns=0;ns<NSSIZE && XAPlay!=XAFeed;ns++)
  {
	 XALastVal=*XAPlay++;
   if(XAPlay==XAEnd) XAPlay=XAStart;

	 l = XALastVal&0xffff;
	 r = (XALastVal>>16) & 0xffff;

   lc=(l * val0 + r * val3) / 128;
   rc=(r * val2 + l * val1) / 128;

	 if( lc < -32768 ) lc = -32768;
	 if( rc < -32768 ) rc = -32768;
	 if( lc > 32767 ) lc = 32767;
	 if( rc > 32767 ) rc = 32767;

	 SSumL[ns]+=lc;
	 SSumR[ns]+=rc;

	 // improve crackle - buffer under
	 // - not update fast enough
	 lastxa_lc = lc;
	 lastxa_rc = rc;


	 // Tales of Phantasia - voice meter
	 if( cdxa_dbuf_ptr >= 0x800 )
		 cdxa_dbuf_ptr = 0;
	 spuMem[ cdxa_dbuf_ptr++ ] = lc;
	 spuMem[ cdxa_dbuf_ptr++ ] = rc;
  }

 if(XAPlay==XAFeed && XARepeat)
  {
   //XARepeat--;
   for(;ns<NSSIZE;ns++)
    {
		 SSumL[ns]+=lastxa_rc;
		 SSumR[ns]+=lastxa_rc;


		 // Tales of Phantasia - voice meter
		 if( cdxa_dbuf_ptr >= 0x800 )
			 cdxa_dbuf_ptr = 0;
		 spuMem[ cdxa_dbuf_ptr++ ] = lastxa_rc;
		 spuMem[ cdxa_dbuf_ptr++ ] = lastxa_rc;
    }
  }

 for(ns=0;ns<NSSIZE && CDDAPlay!=CDDAFeed && (CDDAPlay!=CDDAEnd-1||CDDAFeed!=CDDAStart);ns++)
  {
   cdda_l=*CDDAPlay++;
   if(CDDAPlay==CDDAEnd) CDDAPlay=CDDAStart;

	 l = cdda_l&0xffff;
	 r = (cdda_l>>16) & 0xffff;

   lc=(l * val0 + r * val3) / 128;
   rc=(r * val2 + l * val1) / 128;

	 if( lc < -32768 ) lc = -32768;
	 if( rc < -32768 ) rc = -32768;
	 if( lc > 32767 ) lc = 32767;
	 if( rc > 32767 ) rc = 32767;

	 SSumL[ns]+=lc;
	 SSumR[ns]+=rc;

	 // improve crackle - buffer under
	 // - not update fast enough
	 lastcd_lc = lc;
	 lastcd_rc = rc;
	}


 if(CDDAPlay==CDDAFeed && XARepeat)
  {
   //XARepeat--;
   for(;ns<NSSIZE;ns++)
    {
		 SSumL[ns]+=lastcd_lc;
		 SSumR[ns]+=lastcd_rc;
    }
  }
}

////////////////////////////////////////////////////////////////////////
// small linux time helper... only used for watchdog
////////////////////////////////////////////////////////////////////////

#ifndef _WINDOWS

unsigned long timeGetTime_spu()
{
 struct timeval tv;
 gettimeofday(&tv, 0);                                 // well, maybe there are better ways
 return tv.tv_sec * 1000 + tv.tv_usec/1000;            // to do that, but at least it works
}

#endif

////////////////////////////////////////////////////////////////////////
// FEED XA 
////////////////////////////////////////////////////////////////////////

INLINE void FeedXA(xa_decode_t *xap)
{
 int sinc,spos,i,iSize,iPlace,vl,vr;

 if(!bSPUIsOpen) return;

 xapGlobal = xap;                                      // store info for save states
 XARepeat  = 100;                                      // set up repeat

#ifdef XA_HACK
 iSize=((45500*xap->nsamples)/xap->freq);              // get size
#else
 iSize=((44100*xap->nsamples)/xap->freq);              // get size
#endif
 if(!iSize) return;                                    // none? bye

 if(XAFeed<XAPlay) iPlace=XAPlay-XAFeed;               // how much space in my buf?
 else              iPlace=(XAEnd-XAFeed) + (XAPlay-XAStart);

 if(iPlace==0) return;                                 // no place at all

 //----------------------------------------------------//
 if(iXAPitch)                                          // pitch change option?
  {
   static DWORD dwLT=0;
   static DWORD dwFPS=0;
   static int   iFPSCnt=0;
   static int   iLastSize=0;
   static DWORD dwL1=0;
   DWORD dw=timeGetTime_spu(),dw1,dw2;

   iPlace=iSize;

   dwFPS+=dw-dwLT;iFPSCnt++;

   dwLT=dw;
                                       
   if(iFPSCnt>=10)
    {
     if(!dwFPS) dwFPS=1;
     dw1=1000000/dwFPS; 
     if(dw1>=(dwL1-100) && dw1<=(dwL1+100)) dw1=dwL1;
     else dwL1=dw1;
     dw2=(xap->freq*100/xap->nsamples);
     if((!dw1)||((dw2+100)>=dw1)) iLastSize=0;
     else
      {
       iLastSize=iSize*dw2/dw1;
       if(iLastSize>iPlace) iLastSize=iPlace;
       iSize=iLastSize;
      }
     iFPSCnt=0;dwFPS=0;
    }
   else
    {
     if(iLastSize) iSize=iLastSize;
    }
  }
 //----------------------------------------------------//

 spos=0x10000L;
 sinc = (xap->nsamples << 16) / iSize;                 // calc freq by num / size

 if(xap->stereo)
{
   uint32_t * pS=(uint32_t *)xap->pcm;
   uint32_t l=0;

   if(iXAPitch)
    {
     int32_t l1,l2;short s;
     for(i=0;i<iSize;i++)
      {
       while(spos>=0x10000L)
        {
         l = *pS++;
         spos -= 0x10000L;
        }

       s=(short)LOWORD(l);
       l1=s;
       l1=(l1*iPlace)/iSize;
       if(l1<-32767) l1=-32767;
       if(l1> 32767) l1=32767;
       s=(short)HIWORD(l);
       l2=s;
       l2=(l2*iPlace)/iSize;
       if(l2<-32767) l2=-32767;
       if(l2> 32767) l2=32767;
       l=(l1&0xffff)|(l2<<16);

       *XAFeed++=l;

       if(XAFeed==XAEnd) XAFeed=XAStart;
       if(XAFeed==XAPlay) 
        {
         if(XAPlay!=XAStart) XAFeed=XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
   else
    {
     for(i=0;i<iSize;i++)
      {
       while(spos>=0x10000L)
        {
         l = *pS++;
         spos -= 0x10000L;
        }

       *XAFeed++=l;

       if(XAFeed==XAEnd) XAFeed=XAStart;
       if(XAFeed==XAPlay) 
        {
         if(XAPlay!=XAStart) XAFeed=XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
  }
 else
  {
   unsigned short * pS=(unsigned short *)xap->pcm;
   uint32_t l;short s=0;

   if(iXAPitch)
    {
     int32_t l1;
     for(i=0;i<iSize;i++)
      {
       while(spos>=0x10000L)
        {
         s = *pS++;
         spos -= 0x10000L;
        }
       l1=s;

       l1=(l1*iPlace)/iSize;
       if(l1<-32767) l1=-32767;
       if(l1> 32767) l1=32767;
       l=(l1&0xffff)|(l1<<16);
       *XAFeed++=l;

       if(XAFeed==XAEnd) XAFeed=XAStart;
       if(XAFeed==XAPlay) 
        {
         if(XAPlay!=XAStart) XAFeed=XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
   else
    {
     for(i=0;i<iSize;i++)
      {
       while(spos>=0x10000L)
        {
         s = *pS++;
         spos -= 0x10000L;
        }
       l=s;

       *XAFeed++=(l|(l<<16));

       if(XAFeed==XAEnd) XAFeed=XAStart;
       if(XAFeed==XAPlay) 
        {
         if(XAPlay!=XAStart) XAFeed=XAPlay-1;
         break;
        }

       spos += sinc;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////
// FEED CDDA
////////////////////////////////////////////////////////////////////////

unsigned int cdda_ptr;

INLINE void FeedCDDA(unsigned char *pcm, int nBytes)
{
 while(nBytes>0)
  {
   if(CDDAFeed==CDDAEnd) CDDAFeed=CDDAStart;
   while(CDDAFeed==CDDAPlay-1||
         (CDDAFeed==CDDAEnd-1&&CDDAPlay==CDDAStart))
   {
#ifdef _WINDOWS
    if (!iUseTimer) Sleep(1);
    else return;
#else
    if (!iUseTimer) usleep(1000);
    else return;
#endif
   }
   *CDDAFeed++=(*pcm | (*(pcm+1)<<8) | (*(pcm+2)<<16) | (*(pcm+3)<<24));
   nBytes-=4;
   pcm+=4;

 
#if 0
	 /*
	 Vib Ribbon
	 $00000-$003ff  CD audio left
	 $00400-$007ff  CD audio right
	 */

	 // TIMING: perform in PCSX-r
	 // - gets data from reverb buffer, only update this at intervals (not real-time)

	 // remember: 16-bit ptrs
	 spuMem[ cdda_ptr ] = (l >> 0) & 0xffff;
	 spuMem[ cdda_ptr+0x200 ] = (l >> 16) & 0xffff;

	 cdda_ptr++;
	 cdda_ptr &= 0x1ff;
#endif
 }
}

#endif
