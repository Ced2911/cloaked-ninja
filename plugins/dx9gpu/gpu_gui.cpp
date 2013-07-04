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
// PPDK developer must change libraryName field and can change revision and build
////////////////////////////////////////////////////////////////////////

static const  unsigned char version  = 1;    // do not touch - library for PSEmu 1.x
static const  unsigned char revision = 1;
static const  unsigned char build    = 78;

static char *libraryName     = N_("OpenGL Driver");

static char *PluginAuthor    = N_("Pete Bernert");
static char *libraryInfo     = N_("Based on P.E.Op.S. MesaGL Driver V1.78\nCoded by Pete Bernert\n");

////////////////////////////////////////////////////////////////////////
// stuff to make this a true PDK module
////////////////////////////////////////////////////////////////////////

char * CALLBACK PSEgetLibName(void)
{
	return _(libraryName);
}

unsigned long CALLBACK PSEgetLibType(void)
{
	return  PSE_LT_GPU;
}

unsigned long CALLBACK PSEgetLibVersion(void)
{
	return version<<16|revision<<8|build;
}

char * GPUgetLibInfos(void)
{
	return _(libraryInfo);
}

////////////////////////////////////////////////////////////////////////
// snapshot funcs (saves screen to bitmap / text infos into file)
////////////////////////////////////////////////////////////////////////

char * GetConfigInfos(int hW)
{
	return NULL;
}

////////////////////////////////////////////////////////////////////////
// save text infos to file
////////////////////////////////////////////////////////////////////////

void DoTextSnapShot(int iNum)
{
	return;
}

////////////////////////////////////////////////////////////////////////
// saves screen bitmap to file
////////////////////////////////////////////////////////////////////////

void DoSnapShot(void)
{
}       

void CALLBACK GPUmakeSnapshot(void)
{

}        


////////////////////////////////////////////////////////////////////////
// GPU OPEN: funcs to open up the gpu display (Windows)
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// OPEN interface func: attention! 
// some emus are calling this func in their main Window thread,
// but all other interface funcs (to draw stuff) in a different thread!
// that's a problem, since OGL is thread safe! Therefore we cannot 
// initialize the OGL stuff right here, we simply set a "bIsFirstFrame = TRUE"
// flag, to initialize OGL on the first real draw call.
// btw, we also call this open func ourselfes, each time when the user 
// is changing between fullscreen/window mode (ENTER key)
// btw part 2: in windows the plugin gets the window handle from the
// main emu, and doesn't create it's own window (if it would do it,
// some PAD or SPU plugins would not work anymore)
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT long CALLBACK GPUopen(HWND hwndGPU)                    
{
	HDC hdc;RECT r;
#ifdef _WINDOWS
	hWWindow = hwndGPU;                                   // store hwnd globally
#endif
	ReadConfig();                                       // -> read config from registry
	SetFrameRateConfig();                               // -> setup frame rate stuff

	bIsFirstFrame = TRUE;                                 // flag: we have to init OGL later in windows!
	
	rRatioRect.left   = rRatioRect.top=0;
	rRatioRect.right  = iResX;
	rRatioRect.bottom = iResY;

	r.left=r.top=0;r.right=iResX;r.bottom=iResY;          // hack for getting a clean black window until OGL gets initialized
	/*
	hdc = GetDC(hWWindow);
	FillRect(hdc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
	bSetupPixelFormat(hdc);
	ReleaseDC(hWWindow,hdc);
	*/
	bSetClip=TRUE;

	SetFixes();                                           // setup game fixes

	InitializeTextureStore();                             // init texture mem

	resetGteVertices();

	// lGPUstatusRet = 0x74000000;

	// with some emus, we could do the OGL init right here... oh my
	if(bIsFirstFrame) 
		GLinitialize();

	//glewInit();

	return 0;
}


////////////////////////////////////////////////////////////////////////
// close
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT long GPUclose()                                        // LINUX CLOSE
{
	GLcleanup();                                 // destroy display
	return 0;
}

////////////////////////////////////////////////////////////////////////
// I shot the sheriff... last function called from emu 
////////////////////////////////////////////////////////////////////////

C_ENTRY_POINT long CALLBACK GPUshutdown()
{
	return 0;
}

////////////////////////////////////////////////////////////////////////
// call config dlg
////////////////////////////////////////////////////////////////////////
long CALLBACK GPUconfigure(void)
{

	return 0;
}

////////////////////////////////////////////////////////////////////////
// show about dlg
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUabout(void)
{

}

////////////////////////////////////////////////////////////////////////
// We are ever fine ;)
////////////////////////////////////////////////////////////////////////

long CALLBACK GPUtest(void)
{
	// if test fails this function should return negative value for error (unable to continue)
	// and positive value for warning (can continue but output might be crappy)

	return 0;
}