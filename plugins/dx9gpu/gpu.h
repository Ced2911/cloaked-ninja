/***************************************************************************
                            gpu.h  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    web                  : www.pbernert.com   
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

#ifndef _GPU_INTERNALS_H
#define _GPU_INTERNALS_H

#define PRED(x)   ((x << 3) & 0xF8)
#define PBLUE(x)  ((x >> 2) & 0xF8)
#define PGREEN(x) ((x >> 7) & 0xF8)

#define RED(x) (x & 0xff)
#define BLUE(x) ((x>>16) & 0xff)
#define GREEN(x) ((x>>8) & 0xff)
#define COLOR(x) (x & 0xffffff)

void           DoSnapShot(void);
void           updateDisplay(void);
void           updateFrontDisplay(void);
void           SetAutoFrameCap(void);
void           SetAspectRatio(void);
void           CheckVRamRead(int x, int y, int dx, int dy,BOOL bFront);
void           CheckVRamReadEx(int x, int y, int dx, int dy);
void           SetFixes(void);


typedef struct GPUFREEZETAG
{
	uint32_t ulFreezeVersion;      // should be always 1 for now (set by main emu)
	uint32_t ulStatus;             // current gpu status
	uint32_t ulControl[256];       // latest control register values
	unsigned char psxVRam[1024*1024*2]; // current VRam image (full 2 MB for ZN)
} GPUFreeze_t;

#define C_ENTRY_POINT extern "C" 

#ifdef _XBOX

#define GPUopen			HW_GPUopen
#define GPUdisplayText		HW_GPUdisplayText
#define GPUdisplayFlags		HW_GPUdisplayFlags
#define GPUmakeSnapshot		HW_GPUmakeSnapshot
#define GPUinit			HW_GPUinit
#define GPUclose		HW_GPUclose
#define GPUshutdown		HW_GPUshutdown
#define GPUcursor		HW_GPUcursor
#define GPUupdateLace		HW_GPUupdateLace
#define GPUreadStatus		HW_GPUreadStatus
#define GPUwriteStatus		HW_GPUwriteStatus
#define GPUreadDataMem		HW_GPUreadDataMem
#define GPUreadData		HW_GPUreadData
#define GPUwriteDataMem		HW_GPUwriteDataMem
#define GPUwriteData		HW_GPUwriteData
#define GPUsetMode		HW_GPUsetMode
#define GPUgetMode		HW_GPUgetMode
#define GPUdmaChain		HW_GPUdmaChain
#define GPUconfigure		HW_GPUconfigure
#define GPUabout		HW_GPUabout
#define GPUtest			HW_GPUtest
#define GPUfreeze		HW_GPUfreeze
#define GPUgetScreenPic		HW_GPUgetScreenPic
#define GPUshowScreenPic	HW_GPUshowScreenPic

#define PSEgetLibName           HW_GPUPSEgetLibName
#define PSEgetLibType           HW_GPUPSEgetLibType
#define PSEgetLibVersion        HW_GPUPSEgetLibVersion

#define GPUsetfix               HW_GPUsetfix

#define GPUaddVertex            HW_GPUaddVertex

//new
#define GPUvBlank               HW_GPUvBlank
#define GPUvisualVibration      HW_GPUvisualVibration

#endif

#endif // _GPU_INTERNALS_H
