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

//*************************************************************************//
// History of changes:
//
// 2003/02/18 - kode54
// - added gaussian interpolation
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"

#define _IN_XA

// will be included from spu.c
#ifdef _IN_SPU

////////////////////////////////////////////////////////////////////////
// XA GLOBALS
////////////////////////////////////////////////////////////////////////

xa_decode_t   * xapGlobal=0;

unsigned long * XAFeed  = NULL;
unsigned long * XAPlay  = NULL;
unsigned long * XAStart = NULL;
unsigned long * XAEnd   = NULL;
unsigned long   XARepeat  = 0;
unsigned long   XALastVal = 0;
unsigned long		CDDARepeat = 0;


static FILE *fp_xa_logger;


int             iLeftXAVol  = 0x8000;
int             iRightXAVol = 0x8000;



int xa_gauss_ptr = 0;
int xa_gauss_window[8] = {0, 0, 0, 0, 0, 0, 0, 0};

#define xa_gvall0 xa_gauss_window[xa_gauss_ptr]
#define xa_gvall(x) xa_gauss_window[(xa_gauss_ptr+x)&3]
#define xa_gvalr0 xa_gauss_window[4+xa_gauss_ptr]
#define xa_gvalr(x) xa_gauss_window[4+((xa_gauss_ptr+x)&3)]


int xa_pete_simple_l[5];
int xa_pete_simple_r[5];

#define xa_pete_svall(x) xa_pete_simple_l[x-28]
#define xa_pete_svalr(x) xa_pete_simple_r[x-28]


unsigned int * CDDAFeed  = NULL;
unsigned int * CDDAPlay  = NULL;
unsigned int * CDDAStart = NULL;
unsigned int * CDDAEnd   = NULL;


long cdxa_dbuf_ptr;
int lastxa_lc, lastxa_rc;
int lastcd_lc, lastcd_rc;



INLINE void XASimpleInterpolateUpL(int sinc)
{
	if( sinc == 0 ) sinc=0x10000;
	
	
	if(pete_svall(32)==1)                               // flag == 1? calc step and set flag... and don't change the value in this pass
  {
		const int id1=pete_svall(30)-pete_svall(29);    // curr delta to next val
		const int id2=pete_svall(31)-pete_svall(30);    // and next delta to next-next val :)
		
		pete_svall(32)=0;
		
		if(id1>0)                                           // curr delta positive
    {
			if(id2<id1)
      {pete_svall(28)=id1;pete_svall(32)=2;}
			else
				if(id2<(id1<<1))
					pete_svall(28)=(id1*sinc)/0x10000L;
				else
					pete_svall(28)=(id1*sinc)/0x20000L; 
    }
		else                                                // curr delta negative
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
		if(pete_svall(32)==2)                               // flag 1: calc step and set flag... and don't change the value in this pass
		{
			pete_svall(32)=0;
			
			pete_svall(28)=(pete_svall(28)*sinc)/0x20000L;
			if(sinc<=0x8000)
        pete_svall(29)=pete_svall(30)-(pete_svall(28)*((0x10000/sinc)-1));
			else pete_svall(29)+=pete_svall(28);
		}
		else                                                  // no flags? add bigger val (if possible), calc smaller step, set flag1
			pete_svall(29)+=pete_svall(28);
}


INLINE void XASimpleInterpolateUpR(int sinc)
{
	if( sinc == 0 ) sinc=0x10000;
	
	
	if(pete_svalr(32)==1)                               // flag == 1? calc step and set flag... and don't change the value in this pass
  {
		const int id1=pete_svalr(30)-pete_svalr(29);    // curr delta to next val
		const int id2=pete_svalr(31)-pete_svalr(30);    // and next delta to next-next val :)
		
		pete_svalr(32)=0;
		
		if(id1>0)                                           // curr delta positive
    {
			if(id2<id1)
      {pete_svalr(28)=id1;pete_svalr(32)=2;}
			else
				if(id2<(id1<<1))
					pete_svalr(28)=(id1*sinc)/0x10000L;
				else
					pete_svalr(28)=(id1*sinc)/0x20000L; 
    }
		else                                                // curr delta negative
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
		if(pete_svalr(32)==2)                               // flag 1: calc step and set flag... and don't change the value in this pass
		{
			pete_svalr(32)=0;
			
			pete_svalr(28)=(pete_svalr(28)*sinc)/0x20000L;
			if(sinc<=0x8000)
        pete_svalr(29)=pete_svalr(30)-(pete_svalr(28)*((0x10000/sinc)-1));
			else pete_svalr(29)+=pete_svall(28);
		}
		else                                                  // no flags? add bigger val (if possible), calc smaller step, set flag1
			pete_svalr(29)+=pete_svalr(28);
}



INLINE int XAGetInterpolationVal(int spos, int sinc)
{
	int lc,rc;


	switch(iXAInterp)
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
			y0 = xa_gvall(0);
			y1 = xa_gvall(1);
			y2 = xa_gvall(2);
			y3 = xa_gvall(3);
			
			l_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			l_a2 = ( 2*y0 - 4*y1 + 3*y2 - y3)>>0;
			l_a1 = (-  y0        +   y2     )>>0;
			l_a0 = (        2*y1            )>>0;
			
			// 0.16 fixed-point
			l_val = ((l_a3  ) * mu) >> 16;
			l_val = ((l_a2 + l_val) * mu) >> 16;
			l_val = ((l_a1 + l_val) * mu) >> 16;
			
			lc = l_a0 + (l_val>>0);
			
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = xa_gvalr(0);
			y1 = xa_gvalr(1);
			y2 = xa_gvalr(2);
			y3 = xa_gvalr(3);
			
			r_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			r_a2 = ( 2*y0 - 4*y1 + 3*y2 - y3)>>0;
			r_a1 = (-  y0        +   y2     )>>0;
			r_a0 = (        2*y1            )>>0;
			
			// 0.16 fixed-point
			r_val = ((r_a3  ) * mu) >> 16;
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
			y0 = xa_gvall(0);
			y1 = xa_gvall(1);
			y2 = xa_gvall(2);
			y3 = xa_gvall(3);
			
			l_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			l_a2 = ( 2*y0 - 5*y1 + 4*y2 - y3)>>0;
			l_a1 = (-  y0        +   y2     )>>0;
			l_a0 = (        2*y1            )>>0;
			
			// 0.16 fixed-point
			l_val = ((l_a3  ) * mu) >> 16;
			l_val = ((l_a2 + l_val) * mu) >> 16;
			l_val = ((l_a1 + l_val) * mu) >> 16;
			
			lc = l_a0 + (l_val>>0);
			
			
			
			// y0 = pv4 (old), y1 = pv3, y2 = pv2, y3 = pv1 (new)
			y0 = xa_gvalr(0);
			y1 = xa_gvalr(1);
			y2 = xa_gvalr(2);
			y3 = xa_gvalr(3);
			
			r_a3 = (-  y0 + 3*y1 - 3*y2 + y3)>>0;
			r_a2 = ( 2*y0 - 5*y1 + 4*y2 - y3)>>0;
			r_a1 = (-  y0        +   y2     )>>0;
			r_a0 = (        2*y1            )>>0;
			
			// 0.16 fixed-point
			r_val = ((r_a3  ) * mu) >> 16;
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
			// SPU2-X - Hermite
			
			//#define HERMITE_TENSION 32768/1
			
			int y3,y2,y1,y0,mu;
			int l_val, r_val;
			
			int l_m00,l_m01,l_m0,l_m10,l_m11,l_m1;
			int r_m00,r_m01,r_m0,r_m10,r_m11,r_m1;
			
			mu = spos;
			
			
			y0 = xa_gvall(0);
			y1 = xa_gvall(1);
			y2 = xa_gvall(2);
			y3 = xa_gvall(3);
			
			l_m00 = ((y1-y0)*HERMITE_TENSION) >> 16; // 16.0
			l_m01 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			l_m0  = l_m00 + l_m01;
			
			l_m10 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			l_m11 = ((y3-y2)*HERMITE_TENSION) >> 16; // 16.0
			l_m1  = l_m10 + l_m11;
			
			l_val = ((  2*y1 +   l_m0 + l_m1 - 2*y2) * mu) >> 16; // 16.0
			l_val = ((l_val - 3*y1 - 2*l_m0 - l_m1 + 3*y2) * mu) >> 16; // 16.0
			l_val = ((l_val        +   l_m0            ) * mu) >> 16; // 16.0
			
			lc = l_val + (y1<<0);
			
			
			
			y0 = xa_gvalr(0);
			y1 = xa_gvalr(1);
			y2 = xa_gvalr(2);
			y3 = xa_gvalr(3);
			
			r_m00 = ((y1-y0)*HERMITE_TENSION) >> 16; // 16.0
			r_m01 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			r_m0  = r_m00 + r_m01;
			
			r_m10 = ((y2-y1)*HERMITE_TENSION) >> 16; // 16.0
			r_m11 = ((y3-y2)*HERMITE_TENSION) >> 16; // 16.0
			r_m1  = r_m10 + r_m11;
			
			r_val = ((  2*y1 +   r_m0 + r_m1 - 2*y2) * mu) >> 16; // 16.0
			r_val = ((r_val - 3*y1 - 2*r_m0 - r_m1 + 3*y2) * mu) >> 16; // 16.0
			r_val = ((r_val        +   r_m0            ) * mu) >> 16; // 16.0
			
			rc = r_val + (y1<<0);
    } break;
		//--------------------------------------------------//
	case 4:                                             // cubic interpolation
    {
			int xd;
			
			xd = (spos >> 1)+1;
			
			lc  = xa_gvall(3) - 3*xa_gvall(2) + 3*xa_gvall(1) - xa_gvall(0);
			lc *= (xd - (2<<15)) / 6;
			lc >>= 15;
			lc += xa_gvall(2) - xa_gvall(1) - xa_gvall(1) + xa_gvall(0);
			lc *= (xd - (1<<15)) >> 1;
			lc >>= 15;
			lc += xa_gvall(1) - xa_gvall(0);
			lc *= xd;
			lc >>= 15;
			lc = lc + xa_gvall(0);
			
			
			rc  = xa_gvalr(3) - 3*xa_gvalr(2) + 3*xa_gvalr(1) - xa_gvalr(0);
			rc *= (xd - (2<<15)) / 6;
			rc >>= 15;
			rc += xa_gvalr(2) - xa_gvalr(1) - xa_gvalr(1) + xa_gvalr(0);
			rc *= (xd - (1<<15)) >> 1;
			rc >>= 15;
			rc += xa_gvalr(1) - xa_gvalr(0);
			rc *= xd;
			rc >>= 15;
			rc = rc + xa_gvalr(0);
			
    } break;
		//--------------------------------------------------//
		case 3:
			{
				/*
				ADPCM interpolation (4-tap FIR)
			
				y[n] = (x[n-3] * 4807 + x[n-2] * 22963 + x[n-1] * 4871 - x[n]) >> 15;
				
				- Dr. Hell (Xebra PS1 emu)
				*/
				
				lc = (xa_gvall(3) * 4807 + xa_gvall(2) * 22963 + xa_gvall(1) * 4871 - xa_gvall(0)) >> 15;
				rc = (xa_gvalr(3) * 4807 + xa_gvalr(2) * 22963 + xa_gvalr(1) * 4871 - xa_gvalr(0)) >> 15;
			} break;
			//--------------------------------------------------//
		case 2:                                             // gauss interpolation
			{
				// safety check
				spos &= 0xffff;	

				spos = (spos >> 6) & ~3;
				
				lc=(gauss[spos]*xa_gvall(0)) >> 15;
				lc+=(gauss[spos+1]*xa_gvall(1)) >> 15;
				lc+=(gauss[spos+2]*xa_gvall(2)) >> 15;
				lc+=(gauss[spos+3]*xa_gvall(3)) >> 15;
				
				rc=(gauss[spos]*xa_gvalr(0)) >> 15;
				rc+=(gauss[spos+1]*xa_gvalr(1)) >> 15;
				rc+=(gauss[spos+2]*xa_gvalr(2)) >> 15;
				rc+=(gauss[spos+3]*xa_gvalr(3)) >> 15;
			} break;
			//--------------------------------------------------//
		case 1:                                             // simple interpolation
			{
				XASimpleInterpolateUpL(sinc);                     // --> interpolate up
				XASimpleInterpolateUpR(sinc);                     // --> interpolate up
				
				lc=pete_svall(29);
				rc=pete_svalr(29);
			} break;
			//--------------------------------------------------//
		default:                                            // no interpolation
			{
				lc = xa_gvall0;
				rc = xa_gvalr0;
			} break;
			//--------------------------------------------------//
  }
	
	
	// clip to 16-bits
	lc = CLAMP16(lc);
	rc = CLAMP16(rc);
	
	
	// mask bits again
	// - Megaman Legends 2 (intro FMV)
	lc &= 0xffff;
	rc &= 0xffff;
	
	return lc | (rc<<16);
}


INLINE void XAStoreInterpolationVal(int val_l, int val_r)
{
	if((spuCtrl&0x4000)==0) {
		val_l=0;                       // muted?
		val_r=0;
	}
	
	
	xa_gvall0 = CLAMP16( val_l );
	xa_gvalr0 = CLAMP16( val_r );
	
	
	if(iXAInterp>=2)                            // gauss/cubic interpolation
	{
		xa_gauss_ptr = (xa_gauss_ptr+1) & 3;
	}
	else
		if(iXAInterp==1)                            // simple interpolation
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
			//pChannel->SB[29]=fa;                           // no interpolation
		}
}

////////////////////////////////////////////////////////////////////////
// MIX XA 
////////////////////////////////////////////////////////////////////////

INLINE void MixXA(void)
{
	int ns;
	int lc,rc;
	int decoded_xa;
	
	
	decoded_xa = decoded_ptr;
	
	for(ns=0;ns<APU_run && XAPlay!=XAFeed;ns++)
  {
		XALastVal=*XAPlay++;
		if(XAPlay==XAEnd) XAPlay=XAStart;
		
		lc = (short)(XALastVal&0xffff);
		rc = (short)((XALastVal>>16) & 0xffff);
		
		
		
		// Vib Ribbon - do this here first (ignore real vol control)
		lc = CLAMP16( (lc * iVolXA) / 10 );
		rc = CLAMP16( (rc * iVolXA) / 10 );
		
		
		
		// improve crackle - buffer under
		// - not update fast enough
		lastxa_lc = lc;
		lastxa_rc = rc;
		
		
		// Tales of Phantasia - voice meter
		spuMem[ (decoded_xa + 0x000)/2 ] = (short) lc;
		spuMem[ (decoded_xa + 0x400)/2 ] = (short) rc;
		
		decoded_xa += 2;
		if( decoded_xa >= 0x400 )
			decoded_xa = 0;
		
		

		{
			int voll, volr;


			if( iLeftXAVol & 0x8000 ) voll = ( iLeftXAVol & 0x7fff ) - 0x8000;
			else voll = ( iLeftXAVol & 0x7fff );

			if( iRightXAVol & 0x8000 ) volr = ( iRightXAVol & 0x7fff ) - 0x8000;
			else volr = ( iRightXAVol & 0x7fff );


			// Rayman - stage end fadeout
			lc = CLAMP16( (lc * voll) / 0x8000 );
			rc = CLAMP16( (rc * volr) / 0x8000 );
		}

			
		
		// debug kit
		if( iUseXA == 0 ) {
			lc = 0;
			rc = 0;
		}
		
		
		// reverb write flag
		if( spuCtrl & CTRL_CD_REVERB ) {
			StoreREVERB_CD( lc, rc, ns );
		}
		
		
		// play flag
		if( spuCtrl & CTRL_CD_PLAY ) {
			SSumL[ns]+=lc;
			SSumR[ns]+=rc;
		}
  }
	
	

	if( debug_cdxa_buffer )
	{
		if(!fp_xa_logger){
			fp_xa_logger = fopen( "logger.txt", "w");
		}
		
		if( XAFeed >= XAPlay )
			fprintf( fp_xa_logger, "%d %d\n",
			XAFeed - XAPlay, APU_run - ns );


		if( APU_run != ns ) {
			fprintf( fp_xa_logger, "UNDERFLOW - re-adjust xa\n" );
		}
	}
	
	
	
	if(XAPlay==XAFeed && XARepeat)
  {
		for(;ns<APU_run && XARepeat;ns++, XARepeat--)
    {
			// improve crackle - buffer under
			// - not update fast enough
			lc = lastxa_lc;
			rc = lastxa_rc;
			
			
			// Tales of Phantasia - voice meter
			spuMem[ (decoded_xa + 0x000)/2 ] = (short) lc;
			spuMem[ (decoded_xa + 0x400)/2 ] = (short) rc;
			
			decoded_xa += 2;
			if( decoded_xa >= 0x400 )
				decoded_xa = 0;
			
			
			{
				int voll, volr;


				if( iLeftXAVol & 0x8000 ) voll = ( iLeftXAVol & 0x7fff ) - 0x8000;
				else voll = ( iLeftXAVol & 0x7fff );

				if( iRightXAVol & 0x8000 ) volr = ( iRightXAVol & 0x7fff ) - 0x8000;
				else volr = ( iRightXAVol & 0x7fff );


				// Rayman - stage end fadeout
				lc = CLAMP16( (lc * voll) / 0x8000 );
				rc = CLAMP16( (rc * volr) / 0x8000 );
			}

			
			
			
			// debug kit
			if( iUseXA == 0 ) {
				lc = 0;
				rc = 0;
			}
			
			
			
			// reverb write flags
			if( spuCtrl & CTRL_CD_REVERB ) {
				StoreREVERB_CD( lc, rc, ns );
			}
			
			
			// play flag
			if( spuCtrl & CTRL_CD_PLAY ) {
				SSumL[ns]+=lc;
				SSumR[ns]+=rc;
			}
    }
  }
}


INLINE void MixCDDA(void)
{
	int ns;
	unsigned long cdda_l;
	int lc,rc;
	int decoded_cdda;
	
	
	decoded_cdda = decoded_ptr;
	
	
	//for(ns=0;ns<APU_run && CDDAPlay!=CDDAFeed && (CDDAPlay!=CDDAEnd-1||CDDAFeed!=CDDAStart);ns++)
	for(ns=0;ns<APU_run && CDDAPlay!=CDDAFeed;ns++)
  {
		cdda_l=*CDDAPlay++;
		if(CDDAPlay==CDDAEnd) CDDAPlay=CDDAStart;
		
		lc = (short)(cdda_l&0xffff);
		rc = (short)((cdda_l>>16) & 0xffff);
		
		
		
		// Vib Ribbon - do this first (only possible cd volume)
		lc = CLAMP16( (lc * iVolCDDA) / 10 );
		rc = CLAMP16( (rc * iVolCDDA) / 10 );
		
		
		
		// improve crackle - buffer under
		// - not update fast enough
		lastcd_lc = lc;
		lastcd_rc = rc;
		
		
		
		// Vib Ribbon - playback
		spuMem[ (decoded_cdda + 0x000)/2 ] = (short) lc;
		spuMem[ (decoded_cdda + 0x400)/2 ] = (short) rc;
		
		decoded_cdda += 2;
		if( decoded_cdda >= 0x400 )
			decoded_cdda = 0;
		
		
		
		{
			int voll, volr;


			if( iLeftXAVol & 0x8000 ) voll = ( iLeftXAVol & 0x7fff ) - 0x8000;
			else voll = ( iLeftXAVol & 0x7fff );

			if( iRightXAVol & 0x8000 ) volr = ( iRightXAVol & 0x7fff ) - 0x8000;
			else volr = ( iRightXAVol & 0x7fff );


			// Rayman - stage end fadeout
			lc = CLAMP16( (lc * voll) / 0x8000 );
			rc = CLAMP16( (rc * volr) / 0x8000 );
		}


		
		// debug kit
		if( iUseXA == 0 ) {
			lc = 0;
			rc = 0;
		}


		// reverb write flag
		if( spuCtrl & CTRL_CD_REVERB ) {
			StoreREVERB_CD( lc, rc, ns );
		}


		// play flag
		if( spuCtrl & CTRL_CD_PLAY ) {
			SSumL[ns]+=lc;
			SSumR[ns]+=rc;
		}
	}
	
	
	if( debug_cdxa_buffer )
	{
		if(!fp_xa_logger){
			fp_xa_logger = fopen( "logger.txt", "w");
		}

		if( CDDAFeed >= CDDAPlay )
			fprintf( fp_xa_logger, "%d %d\n",
			CDDAFeed - CDDAPlay, APU_run - ns );


		if( APU_run != ns ) {
			fprintf( fp_xa_logger, "UNDERFLOW - re-adjust xa\n" );
		}
	}



	if(CDDAPlay==CDDAFeed && CDDARepeat)
  {
		for(; ns<APU_run && CDDARepeat; ns++,CDDARepeat--)
    {
			// improve crackle - buffer under
			// - not update fast enough
			lc = lastcd_lc;
			rc = lastcd_rc;
			
			
			// Vib Ribbon - playback
			spuMem[ (decoded_cdda + 0x000)/2 ] = (short) lc;
			spuMem[ (decoded_cdda + 0x400)/2 ] = (short) rc;
			
			decoded_cdda += 2;
			if( decoded_cdda >= 0x400 )
				decoded_cdda = 0;
			
			
			{
				int voll, volr;


				if( iLeftXAVol & 0x8000 ) voll = ( iLeftXAVol & 0x7fff ) - 0x8000;
				else voll = ( iLeftXAVol & 0x7fff );

				if( iRightXAVol & 0x8000 ) volr = ( iRightXAVol & 0x7fff ) - 0x8000;
				else volr = ( iRightXAVol & 0x7fff );


				// Rayman - stage end fadeout
				lc = CLAMP16( (lc * voll) / 0x8000 );
				rc = CLAMP16( (rc * volr) / 0x8000 );
			}



			// debug kit
			if( iUseXA == 0 ) {
				lc = 0;
				rc = 0;
			}
			
			
			
			// reverb write flag
			if( spuCtrl & CTRL_CD_REVERB ) {
				StoreREVERB_CD( lc, rc, ns );
			}
			
			
			// play flag
			if( spuCtrl & CTRL_CD_PLAY ) {
				SSumL[ns]+=lc;
				SSumR[ns]+=rc;
			}
    }
  }
}


////////////////////////////////////////////////////////////////////////
// FEED XA 
////////////////////////////////////////////////////////////////////////

/*
44100 <-- 37800 / 18900

441(00)
- 378(00) = 1.1666666666
- 189(00) = 2.3333333333
	
- 37800 @ 1 - 1/6
- 18900 @ 2 - 2/6
*/
		
INLINE void FeedXA(xa_decode_t *xap)
{
	int i,iSize,iPlace;
	int fa;
	unsigned int spos,sinc;
			
	if(!bSPUIsOpen) return;
			
	xapGlobal = xap;                                      // store info for save states
	XARepeat  = 100;					                               // set up repeat
			
	
	if( xap->freq == 0 ) return;


	iSize=((44100*xap->nsamples)/xap->freq);              // get size
	if(!iSize) return;                                    // none? bye
						
	//----------------------------------------------------//
	if(iUseTimer <= 2 && iXAPitch)                                          // pitch change option?
	{
		static DWORD dwLT=0;
		static DWORD dwFPS=0;
		static int   iFPSCnt=0;
		static int   iLastSize=0;
		static DWORD dwL1=0;
		DWORD dw=timeGetTime(),dw1,dw2;
				
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
			
			
	// (37800 - 18900) / 44100 = 16.16 fixed-point
	//sinc = (xap->nsamples << 16) / iSize;                 // calc freq by num / size
			
			
	// convert to 16.16 fixed-point
	if( xap->freq == 18900 ||
			xap->freq == 37800 ) {
		int len;


		// get buffered amount - dynamic adjust
		if( XAFeed >= XAPlay )
			len = XAFeed - XAPlay;
		else
			len = (XAEnd - XAPlay) + (XAFeed - XAStart);


		/*
		xa runs overclocked - too much underflow

		45100 = overpower typically
		44500 = about average
		44100 = underpower usually
		*/

		if( len == 0 )
			sinc = ((unsigned int)( xap->freq << 16 )) / 44900;
		else if( len >= 200 )
			sinc = ((unsigned int)( xap->freq << 16 )) / 44100;
		else if( len >= 90 )
			sinc = ((unsigned int)( xap->freq << 16 )) / 44300;
		else
			sinc = ((unsigned int)( xap->freq << 16 )) / 44600;


		// safety check
		if( sinc == 0x10000 ) spos = 0;
	}

			

	if( debug_cdxa_buffer )
	{
		if(!fp_xa_logger){
			fp_xa_logger = fopen( "logger.txt", "w");
		}
				
		fprintf( fp_xa_logger, "%X = %d %d %d\n",
			sinc,
			xap->nsamples, xap->freq, iSize );
	}



	if(xap->stereo)
	{
		unsigned long * pS=(unsigned long *)xap->pcm;
		long l=0;

		// stereo + pitch
		if(iUseTimer <= 2 && iXAPitch)
		{
			long l1,l2;short s;

			i = -1;
			while( i < xap->nsamples )
			{
				spos += sinc;

				if(spos >= 0x10000L) {
					// no more data - abort
					i++;
					if( i == xap->nsamples ) break;

					
					l = *pS++;
					spos -= 0x10000L;

					s=(short)(LOWORD(l));
					l1=s;
					l1=(l1*iPlace)/iSize;
							
					s=(short)(HIWORD(l));
					l2=s;
					l2=(l2*iPlace)/iSize;



					XAStoreInterpolationVal(l1,l2);
				}

				if( i == xap->nsamples ) break;


				if( iXAStrength == 0 )
					fa = XAGetInterpolationVal(spos,sinc);
				else
					fa = XAGetInterpolationVal(
						(0x10000 * iXAStrength) / 10, (0x10000 * iXAStrength) / 10);

					
				*XAFeed++=fa;
						
				if(XAFeed==XAEnd) XAFeed=XAStart;
				if(XAFeed==XAPlay)  {
					XAPlay++;
					if( XAPlay==XAEnd ) XAPlay = XAStart;
				}
			} // end samples
		} // end pitch
				
		// stereo - no pitch
		else {
			long l1,l2;


			i = -1;
			while( i < xap->nsamples )
			{
				spos += sinc;

				if(spos >= 0x10000L) {
					// no more data - abort
					i++;
					if( i == xap->nsamples ) break;


					spos -= 0x10000L;

					l = *pS++;
					l1 = (short)(LOWORD(l));
					l2 = (short)(HIWORD(l));


					XAStoreInterpolationVal(l1,l2);
				}


				// no more data - abort
				if( i == xap->nsamples ) break;


				if( iXAStrength == 0 )
					fa = XAGetInterpolationVal(spos,sinc);
				else
					fa = XAGetInterpolationVal(
						(0x10000 * iXAStrength) / 10, (0x10000 * iXAStrength) / 10);	

					
				*XAFeed++=fa;

				if(XAFeed==XAEnd) XAFeed=XAStart;
				if(XAFeed==XAPlay) {
					XAPlay++;
					if( XAPlay==XAEnd ) XAPlay = XAStart;
				}
			} // end samples
		} // end pitch
  }
	else
  {
		unsigned short *pS=(unsigned short *)xap->pcm;
		long l;short s=0;
		
		// mono - pitch adjust
		if(iUseTimer <= 2 && iXAPitch)
    {
			long l1;
			
			i = -1;
			while( i < xap->nsamples )
			{
				spos += sinc;

				if(spos >= 0x10000L) {
					// no more data - abort
					i++;
					if( i == xap->nsamples ) break;

					
					s = *pS++;
					spos -= 0x10000L;
					
					
					l1 = (short) s;
					l1=(l1*iPlace)/iSize;


					XAStoreInterpolationVal(l1,l1);
				}

				if( i == xap->nsamples ) break;

			
				if( iXAStrength == 0 )
					fa = XAGetInterpolationVal(spos,sinc);
				else
					fa = XAGetInterpolationVal(
					(0x10000 * iXAStrength) / 10, (0x10000 * iXAStrength) / 10);

					
				*XAFeed++=fa;
				
				if(XAFeed==XAEnd) XAFeed=XAStart;
				if(XAFeed==XAPlay) 
				{
					XAPlay++;
					if( XAPlay==XAEnd ) XAPlay = XAStart;
				}
			}
    }
		
		// mono - no adjust
		else
		{
			i = -1;
			while( i < xap->nsamples )
			{
				spos += sinc;

				if(spos >= 0x10000L) {
					// no more data - abort
					i++;
					if( i == xap->nsamples ) break;


					s = *pS++;
					spos -= 0x10000L;
					
					l = (short) s;


					XAStoreInterpolationVal(l,l);
				}
				
				if( i == xap->nsamples ) break;
		
				
				if( iXAStrength == 0 )
					fa = XAGetInterpolationVal(spos,sinc);
				else
					fa = XAGetInterpolationVal(
					(0x10000 * iXAStrength) / 10, (0x10000 * iXAStrength) / 10);

					
				*XAFeed++=fa;
				
				if(XAFeed==XAEnd) XAFeed=XAStart;
				if(XAFeed==XAPlay) 
				{
					XAPlay++;
					if( XAPlay==XAEnd ) XAPlay = XAStart;
				}
			} // end samples
		} // end pitch
	} // end stereo
}



////////////////////////////////////////////////////////////////////////
// FEED CDDA
////////////////////////////////////////////////////////////////////////

INLINE void FeedCDDA(unsigned char *pcm, int nBytes)
{
	int l1,l2;
	unsigned short *src;
	int temp;
	int sinc, spos;
	
	
	
	CDDARepeat = 100;
	src = (unsigned short *) pcm;
	
	// reuse XA code
	temp = iXAInterp;
	iXAInterp = iCDDAInterp;
	
	
	
	// 44100 = 16.16 fixed-point
	spos = 0x10000;
			
			
	// convert to 16.16 fixed-point
	{
		unsigned int len, freq;


		// get buffered amount - dynamic adjust
		if( CDDAFeed >= CDDAPlay )
			len = CDDAFeed - CDDAPlay;
		else
			len = (CDDAEnd - CDDAPlay) + (CDDAFeed - CDDAStart);


		/*
		CDDA runs overclocked - some underflow sometimes
		- seems to underflow more often than xa

		44500 = about average
		44100 = normally about average
		*/

		freq = 44100;

		if( len == 0 )
			sinc = ((unsigned int)( freq << 16 )) / 45000;
		else if( len >= 200 )
			sinc = ((unsigned int)( freq << 16 )) / 44100;
		else if( len >= 90 )
			sinc = ((unsigned int)( freq << 16 )) / 44300;
		else
			sinc = ((unsigned int)( freq << 16 )) / 44600;


		// safety check
		if( sinc == 0x10000 ) spos = 0;
	}

	

	if( debug_cdxa_buffer )
	{
		if(!fp_xa_logger){
			fp_xa_logger = fopen( "logger.txt", "w");
		}
				
		fprintf( fp_xa_logger, "%X = %d\n",
			sinc,
			nBytes );
	}



	while(nBytes >= 0)
  {
		spos += sinc;

		if(spos >= 0x10000L) {
			// no more data - abort
			nBytes-=4;
			if( nBytes < 0 ) break;

			spos -= 0x10000;

			l1 = (short)(*src++);
			l2 = (short)(*src++);


			// cdda filter
			XAStoreInterpolationVal(l1,l2);
		}


		// no more data - abort
		if( nBytes < 0 ) break;


		*CDDAFeed++ = XAGetInterpolationVal( (0x10000 * iCDDAStrength) / 10, (0x10000 * iCDDAStrength) / 10);


		if(CDDAFeed==CDDAEnd) CDDAFeed=CDDAStart;
		if(CDDAFeed==CDDAPlay) {
			CDDAPlay++;
			if( CDDAPlay==CDDAEnd ) CDDAPlay = CDDAStart;
		}
  }
	
	
	iXAInterp = temp;
}


#endif