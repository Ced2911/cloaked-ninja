/***************************************************************************
gpu.c  -  description
-------------------
begin                : Sun Mar 08 2009
copyright            : (C) 1999-2009 by Pete Bernert
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

// !!! enable this, if Linux XF86VidMode is not supported: 
//#define NOVMODE

#include "stdafx.h"
#define _IN_GPU

#include "externals.h"
#include "backend.h"
#include "gpu.h"
#include "draw.h"
#include "cfg.h"
#include "prim.h"
#include "psemu_plugin_defs.h"
#include "texture.h"
#include "menu.h"
#include "fps.h"
#include "key.h"
#include "gte_accuracy.h"
#ifdef _WINDOWS
#include "resource.h"
#include "ssave.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(x)  gettext(x)
#define N_(x) (x)
#elif defined(_MACOSX)
#ifdef PCSXRCORE
extern char* Pcsxr_locale_text(char* toloc);
#define _(String) Pcsxr_locale_text(String)
#define N_(String) String
#else
#ifndef PCSXRPLUG
#warning please define the plug being built to use Mac OS X localization!
#define _(msgid) msgid
#define N_(msgid) msgid
#else
//Kludge to get the preprocessor to accept PCSXRPLUG as a variable.
#define PLUGLOC_x(x,y) x ## y
#define PLUGLOC_y(x,y) PLUGLOC_x(x,y)
#define PLUGLOC PLUGLOC_y(PCSXRPLUG,_locale_text)
extern char* PLUGLOC(char* toloc);
#define _(String) PLUGLOC(String)
#define N_(String) String
#endif
#endif
#else
#define _(x)  (x)
#define N_(x) (x)
#endif

#ifdef _MACGL
#include "drawgl.h"
#endif

////////////////////////////////////////////////////////////////////////
// memory image of the PSX vram
////////////////////////////////////////////////////////////////////////

unsigned char  *psxVSecure;
unsigned char  *psxVub;
signed   char  *psxVsb;
unsigned short *psxVuw;
unsigned short *psxVuw_eom;
signed   short *psxVsw;
uint32_t       *psxVul;
signed   int   *psxVsl;

// macro for easy access to packet information
#define GPUCOMMAND(x) ((x>>24) & 0xff)

float         gl_z=0.0f;
BOOL            bNeedInterlaceUpdate=FALSE;
BOOL            bNeedRGB24Update=FALSE;
BOOL            bChangeWinMode=FALSE;

#ifdef _WINDOWS
extern HGLRC    GLCONTEXT;
#endif

uint32_t        ulStatusControl[256];

////////////////////////////////////////////////////////////////////////
// global GPU vars
////////////////////////////////////////////////////////////////////////

static int      GPUdataRet;
int             lGPUstatusRet;

uint32_t        dwGPUVersion = 0;
int             iGPUHeight = 512;
int             iGPUHeightMask = 511;
int             GlobalTextIL = 0;
int             iTileCheat = 0;

static uint32_t      gpuDataM[256];
static unsigned char gpuCommand = 0;
static int           gpuDataC = 0;
static int           gpuDataP = 0;

VRAMLoad_t      VRAMWrite;
VRAMLoad_t      VRAMRead;
int             iDataWriteMode;
int             iDataReadMode;

int             lClearOnSwap;
int             lClearOnSwapColor;
BOOL            bSkipNextFrame = FALSE;
int             iColDepth;
BOOL            bChangeRes;
BOOL            bWindowMode;
int             iWinSize;

// possible psx display widths
short dispWidths[8] = {256,320,512,640,368,384,512,640};

PSXDisplay_t    PSXDisplay;
PSXDisplay_t    PreviousPSXDisplay;
TWin_t          TWin;
short           imageX0,imageX1;
short           imageY0,imageY1;
BOOL            bDisplayNotSet = TRUE;
int             iUseScanLines=0;
float           iScanlineColor[] = {0,0,0, 0.3f}; // easy on the eyes.
int             lSelectedSlot=0;
unsigned char * pGfxCardScreen=0;
int             iRenderFVR=0;
uint32_t        ulGPUInfoVals[16];
int             iFakePrimBusy = 0;
int             iRumbleVal    = 0;
int             iRumbleTime   = 0;
uint32_t        vBlank=0;

////////////////////////////////////////////////////////////////////////
// GPU INIT... here starts it all (first func called by emu)
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT long CALLBACK GPUinit()
{
	memset(ulStatusControl,0,256*sizeof(uint32_t));

	// different ways of accessing PSX VRAM

	psxVSecure=(unsigned char *)malloc((iGPUHeight*2)*1024 + (1024*1024)); // always alloc one extra MB for soft drawing funcs security
	if(!psxVSecure) return -1;

	psxVub=psxVSecure+512*1024;                           // security offset into double sized psx vram!
	psxVsb=(signed char *)psxVub;
	psxVsw=(signed short *)psxVub;
	psxVsl=(signed int *)psxVub;
	psxVuw=(unsigned short *)psxVub;
	psxVul=(uint32_t *)psxVub;

	psxVuw_eom=psxVuw+1024*iGPUHeight;                    // pre-calc of end of vram

	memset(psxVSecure,0x00,(iGPUHeight*2)*1024 + (1024*1024));
	memset(ulGPUInfoVals,0x00,16*sizeof(uint32_t));

	InitFrameCap();                                       // init frame rate stuff

	PSXDisplay.RGB24        = 0;                          // init vars
	PreviousPSXDisplay.RGB24= 0;
	PSXDisplay.Interlaced   = 0;
	PSXDisplay.InterlacedTest=0;
	PSXDisplay.DrawOffset.x = 0;
	PSXDisplay.DrawOffset.y = 0;
	PSXDisplay.DrawArea.x0  = 0;
	PSXDisplay.DrawArea.y0  = 0;
	PSXDisplay.DrawArea.x1  = 320;
	PSXDisplay.DrawArea.y1  = 240;
	PSXDisplay.DisplayMode.x= 320;
	PSXDisplay.DisplayMode.y= 240;
	PSXDisplay.Disabled     = FALSE;
	PreviousPSXDisplay.Range.x0 =0;
	PreviousPSXDisplay.Range.x1 =0;
	PreviousPSXDisplay.Range.y0 =0;
	PreviousPSXDisplay.Range.y1 =0;
	PSXDisplay.Range.x0=0;
	PSXDisplay.Range.x1=0;
	PSXDisplay.Range.y0=0;
	PSXDisplay.Range.y1=0;
	PreviousPSXDisplay.DisplayPosition.x = 1;
	PreviousPSXDisplay.DisplayPosition.y = 1;
	PSXDisplay.DisplayPosition.x = 1;
	PSXDisplay.DisplayPosition.y = 1;
	PreviousPSXDisplay.DisplayModeNew.y=0;
	PSXDisplay.Double=1;
	GPUdataRet=0x400;

	PSXDisplay.DisplayModeNew.x=0;
	PSXDisplay.DisplayModeNew.y=0;

	//PreviousPSXDisplay.Height = PSXDisplay.Height = 239;

	iDataWriteMode = DR_NORMAL;

	// Reset transfer values, to prevent mis-transfer of data
	memset(&VRAMWrite,0,sizeof(VRAMLoad_t));
	memset(&VRAMRead,0,sizeof(VRAMLoad_t));

	// device initialised already !
	//lGPUstatusRet = 0x74000000;
	vBlank = 0;

	STATUSREG = 0x14802000;
	GPUIsIdle;
	GPUIsReadyForCommands;

	return 0;
}                             


////////////////////////////////////////////////////////////////////////
// Update display (swap buffers)... called in interlaced mode on 
// every emulated vsync, otherwise whenever the displayed screen region
// has been changed
////////////////////////////////////////////////////////////////////////

int iLastRGB24=0;                                      // special vars for checking when to skip two display updates
int iSkipTwo=0;

void updateDisplay(void)                               // UPDATE DISPLAY
{
	BOOL bBlur=FALSE;

	bFakeFrontBuffer=FALSE;
	bRenderFrontBuffer=FALSE;

	if(iRenderFVR)                                        // frame buffer read fix mode still active?
	{
		iRenderFVR--;                                       // -> if some frames in a row without read access: turn off mode
		if(!iRenderFVR) bFullVRam=FALSE;
	}

	if(iLastRGB24 && iLastRGB24!=PSXDisplay.RGB24+1)      // (mdec) garbage check
	{
		iSkipTwo=2;                                         // -> skip two frames to avoid garbage if color mode changes
	}
	iLastRGB24=0;

	if(PSXDisplay.RGB24)// && !bNeedUploadAfter)          // (mdec) upload wanted?
	{
		PrepareFullScreenUpload(-1);
		UploadScreen(PSXDisplay.Interlaced);                // -> upload whole screen from psx vram
		bNeedUploadTest=FALSE;
		bNeedInterlaceUpdate=FALSE;
		bNeedUploadAfter=FALSE;
		bNeedRGB24Update=FALSE;
	}
	else {
		if(bNeedInterlaceUpdate)                              // smaller upload?
		{
			bNeedInterlaceUpdate=FALSE;
			xrUploadArea=xrUploadAreaIL;                        // -> upload this rect
			UploadScreen(TRUE);
		}
	}
	if(dwActFixes&512) bCheckFF9G4(NULL);                 // special game fix for FF9 

	if(PreviousPSXDisplay.Range.x0||                      // paint black borders around display area, if needed
		PreviousPSXDisplay.Range.y0)
		PaintBlackBorders();

	if(PSXDisplay.Disabled)                               // display disabled?
	{
		// moved here
		BK_ScissorDisable();                      
		BK_Clear(uiBufferBits, 0, 0, 0, 0xff); 
		BK_ScissorEnable();                       
		gl_z=0.0f;
		bDisplayNotSet = TRUE;
	}

	if(iSkipTwo)                                          // we are in skipping mood?
	{
		iSkipTwo--;
		iDrawnSomething=0;                                  // -> simply lie about something drawn
	}


	if(dwActFixes&128)                                    // special FPS limitation mode?
	{
		if(bUseFrameLimit) PCFrameCap();                    // -> ok, do it
	}



	//----------------------------------------------------//
	// main buffer swapping (well, or skip it)

	if(iDrawnSomething) {
		BK_SwapBuffer();
	}
	iDrawnSomething=0;

	//----------------------------------------------------//

	if(lClearOnSwap)                                      // clear buffer after swap?
	{
		if(bDisplayNotSet)                                  // -> set new vals
			SetOGLDisplaySettings(1);       

		BK_ScissorDisable();                      
		BK_Clear(uiBufferBits, RED(lClearOnSwapColor), GREEN(lClearOnSwapColor), BLUE(lClearOnSwapColor), 0xff); 
		BK_ScissorEnable();     
		lClearOnSwap=0;                                     // -> done
	}
	else 
	{
		//if(iZBufferDepth)                                   // clear zbuffer as well (if activated)
		if (1)
		{
			BK_ScissorDisable();                      
			BK_ClearDepth();
			BK_ScissorEnable();                    
		}
	}
	gl_z=0.0f;

	//----------------------------------------------------//
	// additional uploads immediatly after swapping

	if(bNeedUploadAfter)                                  // upload wanted?
	{
		bNeedUploadAfter=FALSE;                           
		bNeedUploadTest=FALSE;
		UploadScreen(-1);                                   // -> upload
	}

	if(bNeedUploadTest)
	{
		bNeedUploadTest=FALSE;
		if(PSXDisplay.InterlacedTest &&
			//iOffscreenDrawing>2 &&
			PreviousPSXDisplay.DisplayPosition.x==PSXDisplay.DisplayPosition.x &&
			PreviousPSXDisplay.DisplayEnd.x==PSXDisplay.DisplayEnd.x &&
			PreviousPSXDisplay.DisplayPosition.y==PSXDisplay.DisplayPosition.y &&
			PreviousPSXDisplay.DisplayEnd.y==PSXDisplay.DisplayEnd.y)
		{
			PrepareFullScreenUpload(TRUE);
			UploadScreen(TRUE);
		}
	}

}

////////////////////////////////////////////////////////////////////////
// update front display: smaller update func, if something has changed 
// in the frontbuffer... dirty, but hey... real men know no pain
////////////////////////////////////////////////////////////////////////

void updateFrontDisplay(void)
{
	if(PreviousPSXDisplay.Range.x0||
		PreviousPSXDisplay.Range.y0)
		PaintBlackBorders();

	bFakeFrontBuffer=FALSE;
	bRenderFrontBuffer=FALSE;

	if(iDrawnSomething) 
		BK_SwapBuffer();
}

////////////////////////////////////////////////////////////////////////
// check if update needed
////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsX(void)                          // CENTER X
{
	int lx,l;short sO;

	if(!PSXDisplay.Range.x1) return;                      // some range given?

	l=PSXDisplay.DisplayMode.x;

	l*=(int)PSXDisplay.Range.x1;                          // some funky calculation
	l/=2560;lx=l;l&=0xfffffff8;

	if(l==PreviousPSXDisplay.Range.x1) return;            // some change?

	sO=PreviousPSXDisplay.Range.x0;                       // store old

	if(lx>=PSXDisplay.DisplayMode.x)                      // range bigger?
	{
		PreviousPSXDisplay.Range.x1=                        // -> take display width
			PSXDisplay.DisplayMode.x;
		PreviousPSXDisplay.Range.x0=0;                      // -> start pos is 0
	}
	else                                                  // range smaller? center it
	{
		PreviousPSXDisplay.Range.x1=l;                      // -> store width (8 pixel aligned)
		PreviousPSXDisplay.Range.x0=                       // -> calc start pos
			(PSXDisplay.Range.x0-500)/8;
		if(PreviousPSXDisplay.Range.x0<0)                   // -> we don't support neg. values yet
			PreviousPSXDisplay.Range.x0=0;

		if((PreviousPSXDisplay.Range.x0+lx)>                // -> uhuu... that's too much
			PSXDisplay.DisplayMode.x)
		{
			PreviousPSXDisplay.Range.x0=                      // -> adjust start
				PSXDisplay.DisplayMode.x-lx;
			PreviousPSXDisplay.Range.x1+=lx-l;                // -> adjust width
		}                   
	}

	if(sO!=PreviousPSXDisplay.Range.x0)                   // something changed?
	{
		bDisplayNotSet=TRUE;                                // -> recalc display stuff
	}
}

////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsY(void)                          // CENTER Y
{
	int iT;short sO;                                      // store previous y size

	if(PSXDisplay.PAL) iT=48; else iT=28;                 // different offsets on PAL/NTSC

	if(PSXDisplay.Range.y0>=iT)                           // crossed the security line? :)
	{
		PreviousPSXDisplay.Range.y1=                        // -> store width
			PSXDisplay.DisplayModeNew.y;

		sO=(PSXDisplay.Range.y0-iT-4)*PSXDisplay.Double;    // -> calc offset
		if(sO<0) sO=0;

		PSXDisplay.DisplayModeNew.y+=sO;                    // -> add offset to y size, too
	}
	else sO=0;                                            // else no offset

	if(sO!=PreviousPSXDisplay.Range.y0)                   // something changed?
	{
		PreviousPSXDisplay.Range.y0=sO;
		bDisplayNotSet=TRUE;                                // -> recalc display stuff
	}
}


////////////////////////////////////////////////////////////////////////
// big ass check, if an ogl swap buffer is needed
////////////////////////////////////////////////////////////////////////

void updateDisplayIfChanged(void)
{
	BOOL bUp;

	if ((PSXDisplay.DisplayMode.y == PSXDisplay.DisplayModeNew.y) && 
		(PSXDisplay.DisplayMode.x == PSXDisplay.DisplayModeNew.x))
	{
		if((PSXDisplay.RGB24      == PSXDisplay.RGB24New) && 
			(PSXDisplay.Interlaced == PSXDisplay.InterlacedNew)) 
			return;                                          // nothing has changed? fine, no swap buffer needed
	}
	else                                                  // some res change?
	{
		BK_SetView(PSXDisplay.DisplayModeNew.x, PSXDisplay.DisplayModeNew.y);
		if(bKeepRatio) SetAspectRatio();
	}

	bDisplayNotSet = TRUE;                                // re-calc offsets/display area

	bUp=FALSE;
	if(PSXDisplay.RGB24!=PSXDisplay.RGB24New)             // clean up textures, if rgb mode change (usually mdec on/off)
	{
		PreviousPSXDisplay.RGB24=0;                         // no full 24 frame uploaded yet
		ResetTextureArea(FALSE);
		bUp=TRUE;
	}

	PSXDisplay.RGB24         = PSXDisplay.RGB24New;       // get new infos
	PSXDisplay.DisplayMode.y = PSXDisplay.DisplayModeNew.y;
	PSXDisplay.DisplayMode.x = PSXDisplay.DisplayModeNew.x;
	PSXDisplay.Interlaced    = PSXDisplay.InterlacedNew;

	PSXDisplay.DisplayEnd.x=                              // calc new ends
		PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
	PSXDisplay.DisplayEnd.y=
		PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;
	PreviousPSXDisplay.DisplayEnd.x=
		PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
	PreviousPSXDisplay.DisplayEnd.y=
		PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

	ChangeDispOffsetsX();

	if(iFrameLimit==2) SetAutoFrameCap();                 // set new fps limit vals (depends on interlace)

	if(bUp) updateDisplay();                              // yeah, real update (swap buffer)
}

////////////////////////////////////////////////////////////////////////
// swap update check (called by psx vsync function)
////////////////////////////////////////////////////////////////////////

BOOL bSwapCheck(void)
{
	static int iPosCheck=0;
	static PSXPoint_t pO;
	static PSXPoint_t pD;
	static int iDoAgain=0;

	if(PSXDisplay.DisplayPosition.x==pO.x &&
		PSXDisplay.DisplayPosition.y==pO.y &&
		PSXDisplay.DisplayEnd.x==pD.x &&
		PSXDisplay.DisplayEnd.y==pD.y)
		iPosCheck++;
	else iPosCheck=0;

	pO=PSXDisplay.DisplayPosition;
	pD=PSXDisplay.DisplayEnd;

	if(iPosCheck<=4) return FALSE;

	iPosCheck=4;

	if(PSXDisplay.Interlaced) return FALSE;

	if (bNeedInterlaceUpdate||
		bNeedRGB24Update ||
		bNeedUploadAfter|| 
		bNeedUploadTest || 
		iDoAgain
		)
	{
		iDoAgain=0;
		if(bNeedUploadAfter) 
			iDoAgain=1;
		if(bNeedUploadTest && PSXDisplay.InterlacedTest)
			iDoAgain=1;

		bDisplayNotSet = TRUE;
		updateDisplay();

		PreviousPSXDisplay.DisplayPosition.x=PSXDisplay.DisplayPosition.x;
		PreviousPSXDisplay.DisplayPosition.y=PSXDisplay.DisplayPosition.y;
		PreviousPSXDisplay.DisplayEnd.x=PSXDisplay.DisplayEnd.x;
		PreviousPSXDisplay.DisplayEnd.y=PSXDisplay.DisplayEnd.y;
		pO=PSXDisplay.DisplayPosition;
		pD=PSXDisplay.DisplayEnd;

		return TRUE;
	}

	return FALSE;
} 

////////////////////////////////////////////////////////////////////////
// gun cursor func: player=0-7, x=0-511, y=0-255
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUcursor(int iPlayer,int x,int y)
{

}

////////////////////////////////////////////////////////////////////////
// update lace is called every VSync. Basically we limit frame rate 
// here, and in interlaced mode we swap ogl display buffers.
////////////////////////////////////////////////////////////////////////

static unsigned short usFirstPos=2;

C_ENTRY_POINT void CALLBACK GPUupdateLace(void)
{
	//if(!(dwActFixes&0x1000))                               
	// STATUSREG^=0x80000000;                               // interlaced bit toggle, if the CC game fix is not active (see gpuReadStatus)

	if(!(dwActFixes&128))                                 // normal frame limit func
		CheckFrameRate();

	// if(iOffscreenDrawing==4)                              // special check if high offscreen drawing is on
	if(1)
	{
		if(bSwapCheck()) return;
	}

	if(PSXDisplay.Interlaced)                             // interlaced mode?
	{
		STATUSREG^=0x80000000;
		if(PSXDisplay.DisplayMode.x>0 && PSXDisplay.DisplayMode.y>0)
		{
			updateDisplay();                                  // -> swap buffers (new frame)
		}
	}
	else if(bRenderFrontBuffer)                           // no interlace mode? and some stuff in front has changed?
	{
		updateFrontDisplay();                               // -> update front buffer
	}
	else if(usFirstPos==1)                                // initial updates (after startup)
	{
		updateDisplay();
	}
}

////////////////////////////////////////////////////////////////////////
// process read request from GPU status register
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT uint32_t CALLBACK GPUreadStatus(void)
{
	if(dwActFixes&0x1000)                                 // CC game fix
	{
		static int iNumRead=0;
		if((iNumRead++)==2)
		{
			iNumRead=0;
			STATUSREG^=0x80000000;                            // interlaced bit toggle... we do it on every second read status... needed by some games (like ChronoCross)
		}
	}

	if(iFakePrimBusy)                                     // 27.10.2007 - emulating some 'busy' while drawing... pfff... not perfect, but since our emulated dma is not done in an extra thread...
	{
		iFakePrimBusy--;

		if(iFakePrimBusy&1)                                 // we do a busy-idle-busy-idle sequence after/while drawing prims
		{
			GPUIsBusy;
			GPUIsNotReadyForCommands;
		}
		else
		{
			GPUIsIdle;
			GPUIsReadyForCommands;
		}
	}

	return STATUSREG | (vBlank ? 0x80000000 : 0 );;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU status register
// these are always single packet commands.
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUwriteStatus(uint32_t gdata)
{
	uint32_t lCommand=(gdata>>24)&0xff;

	ulStatusControl[lCommand]=gdata;

	switch(lCommand)
	{
		//--------------------------------------------------//
		// reset gpu
	case 0x00:
		memset(ulGPUInfoVals, 0x00, 16 * sizeof(uint32_t));
		lGPUstatusRet = 0x14802000;
		PSXDisplay.Disabled=1;
		iDataWriteMode=iDataReadMode=DR_NORMAL;
		PSXDisplay.DrawOffset.x=PSXDisplay.DrawOffset.y=0;
		drawX=drawY=0;drawW=drawH=0;
		sSetMask=0;lSetMask=0;bCheckMask=FALSE;iSetMask=0;
		usMirror=0;
		GlobalTextAddrX=0;GlobalTextAddrY=0;
		GlobalTextTP=0;GlobalTextABR=0;
		PSXDisplay.RGB24=FALSE;
		PSXDisplay.Interlaced=FALSE;
		bUsingTWin = FALSE;
		return;

		// dis/enable display
	case 0x03:  
		PreviousPSXDisplay.Disabled = PSXDisplay.Disabled;
		PSXDisplay.Disabled = (gdata & 1);

		if(PSXDisplay.Disabled) 
			STATUSREG|=GPUSTATUS_DISPLAYDISABLED;
		else STATUSREG&=~GPUSTATUS_DISPLAYDISABLED;

		//if (iOffscreenDrawing==4 &&
		if(
			PreviousPSXDisplay.Disabled && 
			!(PSXDisplay.Disabled))
		{

			if(!PSXDisplay.RGB24)
			{
				PrepareFullScreenUpload(TRUE);
				UploadScreen(TRUE); 
				updateDisplay();
			}
		}

		return;

		// setting transfer mode
	case 0x04:
		gdata &= 0x03;                                     // only want the lower two bits

		iDataWriteMode=iDataReadMode=DR_NORMAL;
		if(gdata==0x02) iDataWriteMode=DR_VRAMTRANSFER;
		if(gdata==0x03) iDataReadMode =DR_VRAMTRANSFER;

		STATUSREG&=~GPUSTATUS_DMABITS;                     // clear the current settings of the DMA bits
		STATUSREG|=(gdata << 29);                          // set the DMA bits according to the received data

		return;

		// setting display position
	case 0x05: 
		{
			short sx=(short)(gdata & 0x3ff);
			short sy;

			if(iGPUHeight==1024)
			{
				if(dwGPUVersion==2) 
					sy = (short)((gdata>>12)&0x3ff);
				else sy = (short)((gdata>>10)&0x3ff);
			}
			else sy = (short)((gdata>>10)&0x3ff);             // really: 0x1ff, but we adjust it later

			if (sy & 0x200) 
			{
				sy|=0xfc00;
				PreviousPSXDisplay.DisplayModeNew.y=sy/PSXDisplay.Double;
				sy=0;
			}
			else PreviousPSXDisplay.DisplayModeNew.y=0;

			if(sx>1000) sx=0;

			if(usFirstPos)
			{
				usFirstPos--;
				if(usFirstPos)
				{
					PreviousPSXDisplay.DisplayPosition.x = sx;
					PreviousPSXDisplay.DisplayPosition.y = sy;
					PSXDisplay.DisplayPosition.x = sx;
					PSXDisplay.DisplayPosition.y = sy;
				}
			}

			if(dwActFixes&8) 
			{
				if((!PSXDisplay.Interlaced) &&
					PreviousPSXDisplay.DisplayPosition.x == sx  &&
					PreviousPSXDisplay.DisplayPosition.y == sy)
					return;

				PSXDisplay.DisplayPosition.x = PreviousPSXDisplay.DisplayPosition.x;
				PSXDisplay.DisplayPosition.y = PreviousPSXDisplay.DisplayPosition.y;
				PreviousPSXDisplay.DisplayPosition.x = sx;
				PreviousPSXDisplay.DisplayPosition.y = sy;
			}
			else
			{
				if((!PSXDisplay.Interlaced) &&
					PSXDisplay.DisplayPosition.x == sx  &&
					PSXDisplay.DisplayPosition.y == sy)
					return;
				PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
				PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
				PSXDisplay.DisplayPosition.x = sx;
				PSXDisplay.DisplayPosition.y = sy;
			}

			PSXDisplay.DisplayEnd.x=
				PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
			PSXDisplay.DisplayEnd.y=
				PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

			PreviousPSXDisplay.DisplayEnd.x=
				PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
			PreviousPSXDisplay.DisplayEnd.y=
				PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

			bDisplayNotSet = TRUE;

			if (!(PSXDisplay.Interlaced))
			{
				updateDisplay();
			}
			else
				if(PSXDisplay.InterlacedTest && 
					((PreviousPSXDisplay.DisplayPosition.x != PSXDisplay.DisplayPosition.x)||
					(PreviousPSXDisplay.DisplayPosition.y != PSXDisplay.DisplayPosition.y)))
					PSXDisplay.InterlacedTest--;

			return;
		}

		// setting width
	case 0x06:

		PSXDisplay.Range.x0=gdata & 0x7ff;      //0x3ff;
		PSXDisplay.Range.x1=(gdata>>12) & 0xfff;//0x7ff;

		PSXDisplay.Range.x1-=PSXDisplay.Range.x0;

		ChangeDispOffsetsX();

		return;

		// setting height
	case 0x07:

		PreviousPSXDisplay.Height = PSXDisplay.Height;

		PSXDisplay.Range.y0=gdata & 0x3ff;
		PSXDisplay.Range.y1=(gdata>>10) & 0x3ff;

		PSXDisplay.Height = PSXDisplay.Range.y1 - 
			PSXDisplay.Range.y0 +
			PreviousPSXDisplay.DisplayModeNew.y;

		if (PreviousPSXDisplay.Height != PSXDisplay.Height)
		{
			PSXDisplay.DisplayModeNew.y=PSXDisplay.Height*PSXDisplay.Double;
			ChangeDispOffsetsY();
			updateDisplayIfChanged();
		}
		return;

		// setting display infos
	case 0x08:

		PSXDisplay.DisplayModeNew.x = dispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];

		if (gdata&0x04) PSXDisplay.Double=2;
		else            PSXDisplay.Double=1;
		PSXDisplay.DisplayModeNew.y = PSXDisplay.Height*PSXDisplay.Double;

		ChangeDispOffsetsY();

		PSXDisplay.PAL           = (gdata & 0x08)?TRUE:FALSE; // if 1 - PAL mode, else NTSC
		PSXDisplay.RGB24New      = (gdata & 0x10)?TRUE:FALSE; // if 1 - TrueColor
		PSXDisplay.InterlacedNew = (gdata & 0x20)?TRUE:FALSE; // if 1 - Interlace

		STATUSREG&=~GPUSTATUS_WIDTHBITS;                   // clear the width bits

		STATUSREG|=
			(((gdata & 0x03) << 17) | 
			((gdata & 0x40) << 10));                // set the width bits

		PreviousPSXDisplay.InterlacedNew=FALSE;
		if (PSXDisplay.InterlacedNew)
		{
			if(!PSXDisplay.Interlaced)
			{
				PSXDisplay.InterlacedTest=2;
				PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
				PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
				PreviousPSXDisplay.InterlacedNew=TRUE;
			}

			STATUSREG|=GPUSTATUS_INTERLACED;
		}
		else 
		{
			PSXDisplay.InterlacedTest=0;
			STATUSREG&=~GPUSTATUS_INTERLACED;
		}

		if (PSXDisplay.PAL)
			STATUSREG|=GPUSTATUS_PAL;
		else STATUSREG&=~GPUSTATUS_PAL;

		if (PSXDisplay.Double==2)
			STATUSREG|=GPUSTATUS_DOUBLEHEIGHT;
		else STATUSREG&=~GPUSTATUS_DOUBLEHEIGHT;

		if (PSXDisplay.RGB24New)
			STATUSREG|=GPUSTATUS_RGB24;
		else STATUSREG&=~GPUSTATUS_RGB24;

		updateDisplayIfChanged();

		return;

		//--------------------------------------------------//
		// ask about GPU version and other stuff
	case 0x10: 

		gdata&=0xff;

		switch(gdata) 
		{
		case 0x02:
			GPUdataRet=ulGPUInfoVals[INFO_TW];              // tw infos
			return;
		case 0x03:
			GPUdataRet=ulGPUInfoVals[INFO_DRAWSTART];       // draw start
			return;
		case 0x04:
			GPUdataRet=ulGPUInfoVals[INFO_DRAWEND];         // draw end
			return;
		case 0x05:
		case 0x06:
			GPUdataRet=ulGPUInfoVals[INFO_DRAWOFF];         // draw offset
			return;
		case 0x07:
			if(dwGPUVersion==2)
				GPUdataRet=0x01;
			else GPUdataRet=0x02;                           // gpu type
			return;
		case 0x08:
		case 0x0F:                                       // some bios addr?
			GPUdataRet=0xBFC03720;
			return;
		}
		return;
		//--------------------------------------------------//
	}
}

////////////////////////////////////////////////////////////////////////
// vram read/write helpers
////////////////////////////////////////////////////////////////////////

BOOL bNeedWriteUpload=FALSE;

static __inline void FinishedVRAMWrite(void)
{
	if(bNeedWriteUpload)
	{
		bNeedWriteUpload=FALSE;
		CheckWriteUpdate();
	}

	// set register to NORMAL operation
	iDataWriteMode = DR_NORMAL;

	// reset transfer values, to prevent mis-transfer of data
	VRAMWrite.ColsRemaining = 0;
	VRAMWrite.RowsRemaining = 0;
}

static __inline void FinishedVRAMRead(void)
{
	// set register to NORMAL operation
	iDataReadMode = DR_NORMAL;
	// reset transfer values, to prevent mis-transfer of data
	VRAMRead.x = 0;
	VRAMRead.y = 0;
	VRAMRead.Width = 0;
	VRAMRead.Height = 0;
	VRAMRead.ColsRemaining = 0;
	VRAMRead.RowsRemaining = 0;

	// indicate GPU is no longer ready for VRAM data in the STATUS REGISTER
	STATUSREG&=~GPUSTATUS_READYFORVRAM;
}

////////////////////////////////////////////////////////////////////////
// core read from vram
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUreadDataMem(uint32_t *pMem, int iSize)
{
	int i;

	if(iDataReadMode!=DR_VRAMTRANSFER) return;

	GPUIsBusy;

	// adjust read ptr, if necessary
	while(VRAMRead.ImagePtr>=psxVuw_eom)
		VRAMRead.ImagePtr-=iGPUHeight*1024;
	while(VRAMRead.ImagePtr<psxVuw)
		VRAMRead.ImagePtr+=iGPUHeight*1024;

	if((iFrameReadType&1 && iSize>1) &&
		!(iDrawnSomething==2 &&
		VRAMRead.x      == VRAMWrite.x     &&
		VRAMRead.y      == VRAMWrite.y     &&
		VRAMRead.Width  == VRAMWrite.Width &&
		VRAMRead.Height == VRAMWrite.Height))
		CheckVRamRead(VRAMRead.x,VRAMRead.y,
		VRAMRead.x+VRAMRead.RowsRemaining,
		VRAMRead.y+VRAMRead.ColsRemaining,
		TRUE);

	for(i=0;i<iSize;i++)
	{
		// do 2 seperate 16bit reads for compatibility (wrap issues)
		if ((VRAMRead.ColsRemaining > 0) && (VRAMRead.RowsRemaining > 0))
		{
			// lower 16 bit
			GPUdataRet = (uint32_t) GETLE16(VRAMRead.ImagePtr);

			VRAMRead.ImagePtr++;
			if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
			VRAMRead.RowsRemaining --;

			if(VRAMRead.RowsRemaining<=0)
			{
				VRAMRead.RowsRemaining = VRAMRead.Width;
				VRAMRead.ColsRemaining--;
				VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
				if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
			}

			// higher 16 bit (always, even if it's an odd width)
			GPUdataRet |= (uint32_t) GETLE16(VRAMRead.ImagePtr) << 16;
			PUTLE32(pMem, GPUdataRet);
			pMem++;

			if(VRAMRead.ColsRemaining <= 0)
			{FinishedVRAMRead();goto ENDREAD;}

			VRAMRead.ImagePtr++;
			if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
			VRAMRead.RowsRemaining--;
			if(VRAMRead.RowsRemaining<=0)
			{
				VRAMRead.RowsRemaining = VRAMRead.Width;
				VRAMRead.ColsRemaining--;
				VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
				if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
			}
			if(VRAMRead.ColsRemaining <= 0)
			{FinishedVRAMRead();goto ENDREAD;}
		}
		else {FinishedVRAMRead();goto ENDREAD;}
	}

ENDREAD:
	GPUIsIdle;
}

C_ENTRY_POINT uint32_t CALLBACK GPUreadData(void)
{
	uint32_t l;
	GPUreadDataMem(&l,1);
	return GPUdataRet;
}

////////////////////////////////////////////////////////////////////////
// helper table to know how much data is used by drawing commands
////////////////////////////////////////////////////////////////////////

const unsigned char primTableCX[256] =
{
	// 00
	0,0,3,0,0,0,0,0,
	// 08
	0,0,0,0,0,0,0,0,
	// 10
	0,0,0,0,0,0,0,0,
	// 18
	0,0,0,0,0,0,0,0,
	// 20
	4,4,4,4,7,7,7,7,
	// 28
	5,5,5,5,9,9,9,9,
	// 30
	6,6,6,6,9,9,9,9,
	// 38
	8,8,8,8,12,12,12,12,
	// 40
	3,3,3,3,0,0,0,0,
	// 48
	//    5,5,5,5,6,6,6,6,      //FLINE
	254,254,254,254,254,254,254,254,
	// 50
	4,4,4,4,0,0,0,0,
	// 58
	//    7,7,7,7,9,9,9,9,    //    LINEG3    LINEG4
	255,255,255,255,255,255,255,255,
	// 60
	3,3,3,3,4,4,4,4,    //    TILE    SPRT
	// 68
	2,2,2,2,3,3,3,3,    //    TILE1
	// 70
	2,2,2,2,3,3,3,3,
	// 78
	2,2,2,2,3,3,3,3,
	// 80
	4,0,0,0,0,0,0,0,
	// 88
	0,0,0,0,0,0,0,0,
	// 90
	0,0,0,0,0,0,0,0,
	// 98
	0,0,0,0,0,0,0,0,
	// a0
	3,0,0,0,0,0,0,0,
	// a8
	0,0,0,0,0,0,0,0,
	// b0
	0,0,0,0,0,0,0,0,
	// b8
	0,0,0,0,0,0,0,0,
	// c0
	3,0,0,0,0,0,0,0,
	// c8
	0,0,0,0,0,0,0,0,
	// d0
	0,0,0,0,0,0,0,0,
	// d8
	0,0,0,0,0,0,0,0,
	// e0
	0,1,1,1,1,1,1,0,
	// e8
	0,0,0,0,0,0,0,0,
	// f0
	0,0,0,0,0,0,0,0,
	// f8
	0,0,0,0,0,0,0,0
};

////////////////////////////////////////////////////////////////////////
// processes data send to GPU data register
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUwriteDataMem(uint32_t *pMem, int iSize)
{
	unsigned char command;
	uint32_t gdata=0;
	int i=0;
	GPUIsBusy;
	GPUIsNotReadyForCommands;

STARTVRAM:

	if(iDataWriteMode==DR_VRAMTRANSFER)
	{
		// make sure we are in vram
		while(VRAMWrite.ImagePtr>=psxVuw_eom)
			VRAMWrite.ImagePtr-=iGPUHeight*1024;
		while(VRAMWrite.ImagePtr<psxVuw)
			VRAMWrite.ImagePtr+=iGPUHeight*1024;

		// now do the loop
		while(VRAMWrite.ColsRemaining>0)
		{
			while(VRAMWrite.RowsRemaining>0)
			{
				if(i>=iSize) {goto ENDVRAM;}
				i++;

				gdata = GETLE32(pMem);
				pMem++;

				PUTLE16(VRAMWrite.ImagePtr, (unsigned short) gdata);
				VRAMWrite.ImagePtr++;
				if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
				VRAMWrite.RowsRemaining --;

				if(VRAMWrite.RowsRemaining <= 0)
				{
					VRAMWrite.ColsRemaining--;
					if (VRAMWrite.ColsRemaining <= 0)             // last pixel is odd width
					{
						gdata = (gdata & 0xFFFF) | (((uint32_t) GETLE16(VRAMWrite.ImagePtr)) << 16);
						FinishedVRAMWrite();
						goto ENDVRAM;
					}
					VRAMWrite.RowsRemaining = VRAMWrite.Width;
					VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
				}

				PUTLE16(VRAMWrite.ImagePtr, (unsigned short) (gdata >> 16));
				VRAMWrite.ImagePtr++;
				if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
				VRAMWrite.RowsRemaining --;
			}

			VRAMWrite.RowsRemaining = VRAMWrite.Width;
			VRAMWrite.ColsRemaining--;
			VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
		}

		FinishedVRAMWrite();
	}

ENDVRAM:

	if(iDataWriteMode==DR_NORMAL)
	{
		void (* *primFunc)(unsigned char *);
		if(bSkipNextFrame) primFunc=primTableSkip;
		else               primFunc=primTableJ;

		for(;i<iSize;)
		{
			if(iDataWriteMode==DR_VRAMTRANSFER) goto STARTVRAM;

			gdata = GETLE32(pMem);
			pMem++;
			i++;

			if(gpuDataC == 0)
			{
				command = (unsigned char)((gdata>>24) & 0xff);

				if(primTableCX[command])
				{
					gpuDataC = primTableCX[command];
					gpuCommand = command;
					PUTLE32(&gpuDataM[0], gdata);
					gpuDataP = 1;
				}
				else continue;
			}
			else
			{
				PUTLE32(&gpuDataM[gpuDataP], gdata);
				if(gpuDataC>128)
				{
					if((gpuDataC==254 && gpuDataP>=3) ||
						(gpuDataC==255 && gpuDataP>=4 && !(gpuDataP&1)))
					{
						if((gpuDataM[gpuDataP] & 0xF000F000) == 0x50005000)
							gpuDataP=gpuDataC-1;
					}
				}
				gpuDataP++;
			}

			if(gpuDataP == gpuDataC)
			{
				gpuDataC=gpuDataP=0;
				primFunc[gpuCommand]((unsigned char *)gpuDataM);

				if(dwEmuFixes&0x0001 || dwActFixes&0x20000)     // hack for emulating "gpu busy" in some games
					iFakePrimBusy=4;
			}
		} 
	}

	GPUdataRet=gdata;

	GPUIsReadyForCommands;
	GPUIsIdle;                
}

////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUwriteData(uint32_t gdata)
{
	PUTLE32(&gdata, gdata);
	GPUwriteDataMem(&gdata,1);
}

////////////////////////////////////////////////////////////////////////
// sets all kind of act fixes
////////////////////////////////////////////////////////////////////////

void SetFixes(void)
{
	ReInitFrameCap();

	if(dwActFixes & 0x2000) 
		dispWidths[4]=384;
	else dispWidths[4]=368;
}

////////////////////////////////////////////////////////////////////////
// Pete Special: make an 'intelligent' dma chain check (<-Tekken3)
////////////////////////////////////////////////////////////////////////

uint32_t lUsedAddr[3];

static __inline BOOL CheckForEndlessLoop(uint32_t laddr)
{
	if(laddr==lUsedAddr[1]) return TRUE;
	if(laddr==lUsedAddr[2]) return TRUE;

	if(laddr<lUsedAddr[0]) lUsedAddr[1]=laddr;
	else                   lUsedAddr[2]=laddr;
	lUsedAddr[0]=laddr;
	return FALSE;
}

////////////////////////////////////////////////////////////////////////
// core gives a dma chain to gpu: same as the gpuwrite interface funcs
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT long CALLBACK GPUdmaChain(uint32_t *baseAddrL, uint32_t addr)
{
	uint32_t dmaMem;
	unsigned char * baseAddrB;
	short count;unsigned int DMACommandCounter = 0;

	GPUIsBusy;

	lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

	baseAddrB = (unsigned char*) baseAddrL;

	do
	{
		if(iGPUHeight==512) addr&=0x1FFFFC;

		if(DMACommandCounter++ > 2000000) break;
		if(CheckForEndlessLoop(addr)) break;

		count = baseAddrB[addr+3];

		dmaMem=addr+4;

		if(count>0) GPUwriteDataMem(&baseAddrL[dmaMem>>2],count);

		addr = GETLE32(&baseAddrL[addr >> 2])&0xffffff;
	}
	while (addr != 0xffffff);

	GPUIsIdle;

	return 0;
}

////////////////////////////////////////////////////////////////////////
// save state funcs
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT long CALLBACK GPUfreeze(uint32_t ulGetFreezeData,GPUFreeze_t * pF)
{
	if(ulGetFreezeData==2) 
	{
		int lSlotNum=*((int *)pF);
		if(lSlotNum<0) return 0;
		if(lSlotNum>8) return 0;
		lSelectedSlot=lSlotNum+1;
		return 1;
	}

	if(!pF)                    return 0; 
	if(pF->ulFreezeVersion!=1) return 0;

	if(ulGetFreezeData==1)
	{
		pF->ulStatus=STATUSREG;
		memcpy(pF->ulControl,ulStatusControl,256*sizeof(uint32_t));
		memcpy(pF->psxVRam,  psxVub,         1024*iGPUHeight*2);

		return 1;
	}

	if(ulGetFreezeData!=0) return 0;

	STATUSREG=pF->ulStatus;
	memcpy(ulStatusControl,pF->ulControl,256*sizeof(uint32_t));
	memcpy(psxVub,         pF->psxVRam,  1024*iGPUHeight*2);

	ResetTextureArea(TRUE);

	GPUwriteStatus(ulStatusControl[0]);
	GPUwriteStatus(ulStatusControl[1]);
	GPUwriteStatus(ulStatusControl[2]);
	GPUwriteStatus(ulStatusControl[3]);
	GPUwriteStatus(ulStatusControl[8]);
	GPUwriteStatus(ulStatusControl[6]);
	GPUwriteStatus(ulStatusControl[7]);
	GPUwriteStatus(ulStatusControl[5]);
	GPUwriteStatus(ulStatusControl[4]);

	return 1;
}

////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUgetScreenPic(unsigned char * pMem)
{

}

////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUshowScreenPic(unsigned char * pMem)
{

}

////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUsetfix(uint32_t dwFixBits)
{
}

////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUvisualVibration(uint32_t iSmall, uint32_t iBig)
{
}

////////////////////////////////////////////////////////////////////////
// main emu can set display infos (A/M/G/D) 
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT void CALLBACK GPUdisplayFlags(uint32_t dwFlags)
{

}

C_ENTRY_POINT void CALLBACK GPUvBlank( int val )
{
	vBlank = val;
}
