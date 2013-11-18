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

//////////////////////////////////////////////////////////
// WINDOWS
//////////////////////////////////////////////////////////

#ifdef _WINDOWS

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include "mmsystem.h"
#include <process.h>
#include <stdlib.h>

// enable that for auxprintf();
//#define SMALLDEBUG
//#include <dbgout.h>
//void auxprintf (LPCTSTR pFormat, ...);

#define INLINE __inline

//////////////////////////////////////////////////////////
// LINUX
//////////////////////////////////////////////////////////
#elif defined(_XBOX)
#include <xtl.h>
#include <process.h>
#include <stdlib.h>
#define INLINE __inline
#define NOTHREADLIB
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

#if 1
#define SPUreadDMA			PEOPS_SPUreadDMA
#define SPUreadDMAMem			PEOPS_SPUreadDMAMem
#define SPUwriteDMA			PEOPS_SPUwriteDMA
#define SPUwriteDMAMem			PEOPS_SPUwriteDMAMem
#define SPUasync			PEOPS_SPUasync
#define SPUupdate			PEOPS_SPUupdate
#define SPUplayADPCMchannel		PEOPS_SPUplayADPCMchannel
#define SPUinit				PEOPS_SPUinit
#define SPUopen				PEOPS_SPUopen
#define SPUsetConfigFile		PEOPS_SPUsetConfigFile
#define SPUclose			PEOPS_SPUclose
#define SPUshutdown			PEOPS_SPUshutdown
#define SPUtest				PEOPS_SPUtest
#define SPUconfigure			PEOPS_SPUconfigure
#define SPUabout			PEOPS_SPUabout
#define SPUregisterCallback		PEOPS_SPUregisterCallback
#define SPUregisterCDDAVolume           PEOPS_SPUregisterCDDAVolume
#define SPUwriteRegister		PEOPS_SPUwriteRegister
#define SPUreadRegister			PEOPS_SPUreadRegister
#define SPUfreeze			PEOPS_SPUfreeze
#define SPUplayCDDAchannel              PEOPS_SPUplayCDDAchannel
#define SPUsetframelimit                PEOPS_SPUsetframelimit

#define ReadConfig			SPUReadConfig
#define ReadConfigFile			SPUReadConfigFile
#define iDebugMode			SPUiDebugMode

#define PSEgetLibName           SPUPSEgetLibName
#define PSEgetLibType           SPUPSEgetLibType
#define PSEgetLibVersion        SPUPSEgetLibVersion

#define build                   SPUbuild
#define revision                SPUrevision
#define version                 SPUversion

#define TR {printf("[Trace] in function %s, line %d, file %s\n",__FUNCTION__,__LINE__,__FILE__);}

#endif