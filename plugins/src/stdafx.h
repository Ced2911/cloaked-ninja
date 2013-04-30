/***************************************************************************
                           StdAfx.h  -  description
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

#ifdef _XBOX
#define SPUreadDMA				PEOPS110_SPUreadDMA
#define SPUreadDMAMem			PEOPS110_SPUreadDMAMem
#define SPUwriteDMA				PEOPS110_SPUwriteDMA
#define SPUwriteDMAMem			PEOPS110_SPUwriteDMAMem
#define SPUasync				PEOPS110_SPUasync
#define SPUupdate				PEOPS110_SPUupdate
#define SPUplayADPCMchannel		PEOPS110_SPUplayADPCMchannel
#define SPUinit					PEOPS110_SPUinit
#define SPUopen					PEOPS110_SPUopen
#define SPUsetConfigFile		PEOPS110_SPUsetConfigFile
#define SPUclose				PEOPS110_SPUclose
#define SPUshutdown				PEOPS110_SPUshutdown
#define SPUtest					PEOPS110_SPUtest
#define SPUconfigure			PEOPS110_SPUconfigure
#define SPUabout				PEOPS110_SPUabout
#define SPUregisterCallback		PEOPS110_SPUregisterCallback
#define SPUregisterCDDAVolume	PEOPS110_SPUregisterCDDAVolume
#define SPUwriteRegister		PEOPS110_SPUwriteRegister
#define SPUreadRegister			PEOPS110_SPUreadRegister
#define SPUfreeze				PEOPS110_SPUfreeze

#define ReadConfig				SPU110ReadConfig
#define ReadConfigFile			SPU110ReadConfigFile
#define iDebugMode				SPU110iDebugMode
#endif

//////////////////////////////////////////////////////////
// WINDOWS
//////////////////////////////////////////////////////////

#if defined(_WINDOWS) || defined(_XBOX)

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include "mmsystem.h"
#include <process.h>
#else
#include <xtl.h>
#endif
#include <stdlib.h>

// enable that for auxprintf();
//#define SMALLDEBUG
//#include <dbgout.h>
//void auxprintf (LPCTSTR pFormat, ...);

#define INLINE __inline
#define CALLBACK __cdecl

//////////////////////////////////////////////////////////
// LINUX
//////////////////////////////////////////////////////////
#else

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#ifndef NOTHREADLIB
#include <pthread.h>
#endif
#define RRand(range) (random()%range)  
#include <string.h> 
#include <sys/time.h>  
#include <math.h>  

#undef CALLBACK
#define CALLBACK
#define DWORD unsigned long
#define LOWORD(l)           ((unsigned short)(l)) 
#define HIWORD(l)           ((unsigned short)(((unsigned long)(l) >> 16) & 0xFFFF)) 

#define INLINE inline

#endif

#include "psemuxa.h"
