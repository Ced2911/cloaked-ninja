/***************************************************************************
                         externals.h  -  description
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
// 2002/04/04 - Pete
// - increased channel struct for interpolation
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//


/////////////////////////////////////////////////////////
// generic defines
/////////////////////////////////////////////////////////

#define PSE_LT_SPU                  4
#define PSE_SPU_ERR_SUCCESS         0
#define PSE_SPU_ERR                 -60
#define PSE_SPU_ERR_NOTCONFIGURED   PSE_SPU_ERR - 1
#define PSE_SPU_ERR_INIT            PSE_SPU_ERR - 2
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif


// 15-bit value + 1-sign
__inline int CLAMP16( int x ) {
	if(x > 32767) x = 32767;
	else if(x < -32768) x = -32768;

	return x;
}


#define HERMITE_TENSION 32768*3/4

////////////////////////////////////////////////////////////////////////
// spu defines
////////////////////////////////////////////////////////////////////////

// ~ 1 ms of data - somewhat slower than Eternal
//#define NSSIZE 45
//#define INTERVAL_TIME 1000

// ~ 0.5 ms of data - roughly Eternal maybe
//#define NSSIZE 23
//#define INTERVAL_TIME 2000

// ~ 0.25 ms of data - seems a little bad..?
//#define NSSIZE 12
//#define INTERVAL_TIME 4000

//#define NSSIZE 10
//#define APU_CYCLES_UPDATE (NSSIZE * 4)


// maximum allowed
#define NSSIZE 100


/*
async
- novastorm
- megaman legends 1/2

games need tight sync
- voice moves at same pace with mem writes
- best emu cycle timing
*/




// sound buffer sizes


/*
NOTE:
- 1 ms = XP / Vista
- 2 ms = Vista / Win 7 (more overhead)
*/


extern int output_channels;
extern int output_samplesize;



// ~1 ms buffer amounts
#define SOUNDLEN(x)		(((44100 * (x)) / 1000) * output_samplesize)


// complete sound buffer (~24 sec) @ 7.1 upmix
#define SOUNDSIZE			0x20 * 0x100000
#define UPLOADSIZE		SOUNDLEN( upload_timer )


// test buffer... if less than that is buffered, a new upload will happen
#define LATENCY				latency_target

#define TESTMIN				SOUNDLEN( upload_low_reset )

#define TESTMAX				SOUNDLEN( LATENCY + upload_high_full )
#define FULLMAX				SOUNDLEN( LATENCY + upload_high_reset )


// num of channels
#define MAXCHAN     24

///////////////////////////////////////////////////////////
// struct defines
///////////////////////////////////////////////////////////

// ADSR INFOS PER CHANNEL
typedef struct
{
 int            AttackModeExp;
 long           AttackTime;
 long           DecayTime;
 long           SustainLevel;
 int            SustainModeExp;
 long           SustainModeDec;
 long           SustainTime;
 int            ReleaseModeExp;
 unsigned long  ReleaseVal;
 long           ReleaseTime;
 long           ReleaseStartTime; 
 long           ReleaseVol; 
 long           lTime;
 long           lVolume;
} ADSRInfo;

typedef struct
{
 int            State;
 int            AttackModeExp;
 int            AttackRate;
 int            DecayRate;
 int            SustainLevel;
 int            SustainModeExp;
 int            SustainIncrease;
 int            SustainRate;
 int            ReleaseModeExp;
 int            ReleaseRate;
 int            EnvelopeVol;
 int            EnvelopeVol_f;			// fraction
 long           lVolume;
 long           StartDelay;
 long           lDummy2;
} ADSRInfoEx;
              
///////////////////////////////////////////////////////////

// Tmp Flags

// used for debug channel muting
#define FLAG_MUTE  1

// used for simple interpolation
#define FLAG_IPOL0 2
#define FLAG_IPOL1 4

///////////////////////////////////////////////////////////

// MAIN CHANNEL STRUCT
typedef struct
{
 // no mutexes used anymore... don't need them to sync access
 //HANDLE            hMutex;

 int               bNew;                               // start flag

 int               iSBPos;                             // mixing stuff
 int               spos;
 int               sinc;
 int               SB[32+32];                          // Pete added another 32 dwords in 1.6 ... prevents overflow issues with gaussian/cubic interpolation (thanx xodnizel!), and can be used for even better interpolations, eh? :)
 int               sval;

 unsigned char *   pStart;                             // start ptr into sound mem
 unsigned char *   pCurr;                              // current pos in sound mem
 unsigned char *   pLoop;                              // loop ptr in sound mem

 int               bOn;                                // is channel active (sample playing?)
 int               bStop;                              // is channel stopped (sample _can_ still be playing, ADSR Release phase)
 int               bReverb;                            // can we do reverb on this channel? must have ctrl register bit, to get active
 int               iActFreq;                           // current psx pitch
 int               iUsedFreq;                          // current pc pitch
 int               iLeftVolume;                        // left volume
 int               iLeftVolRaw;                        // left psx volume value
 int               bLoopJump;	                         // loop jump bit
 int               iMute;                              // mute mode (debug)
 int               iSilent;                            // voice on - sound on/off
 int               iRightVolume;                       // right volume
 int               iRightVolRaw;                       // right psx volume value
 int               iRawPitch;                          // raw pitch (0...3fff)
 int               iIrqDone;                           // debug irq done flag
 int               s_1;                                // last decoding infos
 int               s_2;
 int               bRVBActive;                         // reverb active flag
 int               iRVBOffset;                         // reverb offset
 int               iRVBRepeat;                         // reverb repeat
 int               bNoise;                             // noise active flag
 int               bFMod;                              // freq mod (0=off, 1=sound channel, 2=freq channel)
 int               iRVBNum;                            // another reverb helper
 int               iOldNoise;                          // old noise val for this channel   
 ADSRInfo          ADSR;                               // active ADSR settings
 ADSRInfoEx        ADSRX;                              // next ADSR settings (will be moved to active on sample start)
} SPUCHAN;

///////////////////////////////////////////////////////////

typedef struct
{
 int StartAddr;      // reverb area start addr in samples
 int CurrAddr;       // reverb area curr addr in samples

 int VolLeft;
 int VolRight;
 int iLastRVBLeft;
 int iLastRVBRight;
 int iRVBLeft;
 int iRVBRight;


 int FB_SRC_A;       // (offset)
 int FB_SRC_B;       // (offset)
 int IIR_ALPHA;      // (coef.)
 int ACC_COEF_A;     // (coef.)
 int ACC_COEF_B;     // (coef.)
 int ACC_COEF_C;     // (coef.)
 int ACC_COEF_D;     // (coef.)
 int IIR_COEF;       // (coef.)
 int FB_ALPHA;       // (coef.)
 int FB_X;           // (coef.)
 int IIR_DEST_A0;    // (offset)
 int IIR_DEST_A1;    // (offset)
 int ACC_SRC_A0;     // (offset)
 int ACC_SRC_A1;     // (offset)
 int ACC_SRC_B0;     // (offset)
 int ACC_SRC_B1;     // (offset)
 int IIR_SRC_A0;     // (offset)
 int IIR_SRC_A1;     // (offset)
 int IIR_DEST_B0;    // (offset)
 int IIR_DEST_B1;    // (offset)
 int ACC_SRC_C0;     // (offset)
 int ACC_SRC_C1;     // (offset)
 int ACC_SRC_D0;     // (offset)
 int ACC_SRC_D1;     // (offset)
 int IIR_SRC_B1;     // (offset)
 int IIR_SRC_B0;     // (offset)
 int MIX_DEST_A0;    // (offset)
 int MIX_DEST_A1;    // (offset)
 int MIX_DEST_B0;    // (offset)
 int MIX_DEST_B1;    // (offset)
 int IN_COEF_L;      // (coef.)
 int IN_COEF_R;      // (coef.)
} REVERBInfo;

#ifdef _WINDOWS
extern HINSTANCE hInst;
#define WM_MUTE (WM_USER+543)
#endif

///////////////////////////////////////////////////////////
// SPU.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_SPU

extern int iZincEmu;



// new settings
extern int phantom_padder;
extern int phantom_pad_size;
extern int phantom_post_pad;
extern int upload_timer;
extern int latency_target;
extern int latency_restart;
extern int upload_low_reset;
extern int upload_high_full;
extern int upload_high_reset;
extern int async_wait_block;
extern int async_ondemand_block;
extern int debug_sound_buffer;
extern int debug_cdxa_buffer;
extern int sound_stretcher;
extern int reverb_target;


// psx buffers / addresses
extern unsigned short  regArea[];                        
extern unsigned short  spuMem[];
extern unsigned char * spuMemC;
extern unsigned char * pSpuIrq;
extern unsigned char * pSpuBuffer;

// user settings

extern int				iOutputDriver;
extern int        iUseXA;
extern int        iVolume;
extern int        iXAPitch;
extern int        iUseTimer;
extern int        iSPUIRQWait;
extern int        iDebugMode;
extern int        iRecordMode;
extern int        iUseReverb;
extern int        iUseInterpolation;
extern int        iDisStereo;
extern int        iUseDBufIrq;
extern int				iFreqResponse;
extern int				iEmuType;
extern int				iReleaseIrq;
extern int				iReverbBoost;

extern int				iLatency;
extern int				iXAInterp;
extern int				iCDDAInterp;
extern int				iOutputInterp1;
extern int				iOutputInterp2;
extern int				iVolCDDA;
extern int				iVolXA;
extern int				iVolVoices;
extern int				iVolMainL;
extern int				iVolMainR;

extern int				iXAStrength;
extern int				iCDDAStrength;
extern int				iOutput2Strength;

// MISC

extern SPUCHAN s_chan[];
extern REVERBInfo rvb;

extern unsigned long dwNoiseVal;
extern unsigned long dwNoiseClock;
extern unsigned long dwNoiseCount;
extern unsigned short spuCtrl;
extern unsigned short spuStat;
extern unsigned short spuIrq;
extern unsigned long  spuAddr;
extern int      bEndThread; 
extern int      bThreadEnded;
extern int      bSpuInit;
extern unsigned long dwNewChannel;
extern int bIrqHit;

extern long cpu_cycles;
extern long cpu_clock;
extern long	total_cpu_cycles;
extern long	total_apu_cycles;

extern long APU_run;

extern int      SSumR[];
extern int      SSumL[];
extern int      iCycle;
extern short *  pS;

extern long voice_dbuf_ptr;
extern long cdxa_dbuf_ptr;
extern int lastxa_lc, lastxa_rc;
extern int lastcd_lc, lastcd_rc;

extern int iSpuAsyncWait;

#ifdef _WINDOWS
extern HWND    hWMain;                               // window handle
extern HWND    hWDebug;
#endif

extern void (CALLBACK *cddavCallback)(unsigned short,unsigned short);
extern void (CALLBACK *irqCallback)(void);                  // func of main emu, called on spu irq


extern int out_gauss_window[];
extern int framelimiter;



#endif

///////////////////////////////////////////////////////////
// CFG.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_CFG

#ifndef _WINDOWS
extern char * pConfigFile;
#endif

#endif

///////////////////////////////////////////////////////////
// DSOUND.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_DSOUND

#if defined(_WINDOWS) || defined(_XBOX)
extern unsigned long LastWrite;
extern unsigned long LastPlay;
#endif


extern int debug_sound_buffer;
extern unsigned long LastPad;


#endif

///////////////////////////////////////////////////////////
// RECORD.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_RECORD

#ifdef _WINDOWS
extern int iDoRecord;
#endif

#endif

///////////////////////////////////////////////////////////
// XA.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_XA

extern xa_decode_t   * xapGlobal;

extern unsigned long * XAFeed;
extern unsigned long * XAPlay;
extern unsigned long * XAStart;
extern unsigned long * XAEnd;

extern unsigned long   XARepeat;
extern unsigned long   XALastVal;
extern unsigned long	 CDDARepeat;

extern int           iLeftXAVol;
extern int           iRightXAVol;

extern unsigned int * CDDAFeed;
extern unsigned int * CDDAPlay;
extern unsigned int * CDDAStart;
extern unsigned int * CDDAEnd;

extern int xa_gauss_window[];

#endif

///////////////////////////////////////////////////////////
// REVERB.C globals
///////////////////////////////////////////////////////////

#ifndef _IN_REVERB

extern int *          sRVBPlay;
extern int *          sRVBEnd;
extern int *          sRVBStart;
extern int            iReverbOff;
extern int            iReverbRepeat;
extern int            iReverbNum;    

#endif
