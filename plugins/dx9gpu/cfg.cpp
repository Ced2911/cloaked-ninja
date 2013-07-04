/***************************************************************************
                           cfg.c  -  description
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

#define _IN_CFG

/////////////////////////////////////////////////////////////////////////////
// Windows config handling

#include "stdafx.h"
#include "externals.h"
#include "cfg.h"
#include <stdio.h>

void ReadConfig(void)                                  // read all config vals
{
 // pre-init all relevant globals

 iResX=1280;iResY=720;
 iColDepth=32;
 bChangeRes=FALSE;
 bWindowMode=FALSE;
 bFullVRam=TRUE;
 iFilterType=0;
 bDrawDither=FALSE;
 bUseFrameLimit=FALSE;
 bUseFrameSkip=FALSE;
 iFrameLimit=0;
 fFrameRate=200.0f;
 bOpaquePass=TRUE;
 iTexQuality=0;
 iWinSize=MAKELONG(400,300);
 iUseMask=1;
 iZBufferDepth=0;
 bUseFastMdec=FALSE;
 bUse15bitMdec=FALSE;
 dwCfgFixes=0;
 bUseFixes=FALSE;
 bGteAccuracy=FALSE;
 iUseScanLines=0;
 iFrameTexType=0; // 0 for crono cross
 iFrameReadType=0;
 bKeepRatio=FALSE;
 bForceRatio43=FALSE;
 iVRamSize=0;
 iTexGarbageCollection=1;
 iHiResTextures=0;
 if(!iColDepth)        iColDepth=32;                   // adjust color info
 if(iUseMask)          iZBufferDepth=16;               // set zbuffer depth 
 else                  iZBufferDepth=0;
 if(bUseFixes)         dwActFixes=dwCfgFixes;          // init game fix global
 if(iFrameReadType==4) bFullVRam=TRUE;                 // set fullvram render flag (soft gpu)
}

////////////////////////////////////////////////////////////////////////
// read registry: window size (called on fullscreen/window resize)
////////////////////////////////////////////////////////////////////////

void ReadWinSizeConfig(void)
{
	/*
 iResX=800;
 iResY=600;
 */
}
 
////////////////////////////////////////////////////////////////////////
// write registry
////////////////////////////////////////////////////////////////////////

void WriteConfig(void)
{
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// Key definition window
