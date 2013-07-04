/***************************************************************************
                          stdafx.h  -  description
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <mmsystem.h>
#include "resource.h"
#endif

#pragma warning (disable:4244)
#pragma warning (disable:4761)
/*
#include <GL/glew.h>
#include <GL/gl.h>
*/

#define SHADETEXBIT(x) ((x>>24) & 0x1)
#define SEMITRANSBIT(x) ((x>>25) & 0x1)

#ifndef _WINDOWS
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT GL_BGRA
#endif
#define GL_COLOR_INDEX8_EXT 0x80E5
#endif


#define GETLEs16(X) ((int16_t)GETLE16((uint16_t *)X))
#define GETLEs32(X) ((int16_t)GETLE32((uint16_t *)X))

#ifdef _XBOX
#define SWAP16(x) _byteswap_ushort(x)
#define SWAP32(x) _byteswap_ulong(x)
#else
#define SWAP16(x)	x
#define SWAP32(x)	x
#endif

#ifdef _XBOX
#define GETLE32(X) __loadwordbytereverse(0,(X))
#define GETLE16(X) __loadshortbytereverse(0,(X))
#define  PUTLE16(X,Y) __storeshortbytereverse(Y,0,(X));
#define  PUTLE32(X,Y) __storewordbytereverse(Y,0,(X));
#else
#define GETLE16(X) (*(uint16_t *)X)
#define GETLE32(X) (*(uint32_t *)X)
#define GETLE16D(X) ({uint32_t val = GETLE32(X); (val<<16 | val >> 16);})
#define PUTLE16(X, Y) *((uint16_t *)X)=((uint16_t)Y);
#define PUTLE32(X, Y) *((uint32_t *)X)=((uint32_t)Y);
#endif