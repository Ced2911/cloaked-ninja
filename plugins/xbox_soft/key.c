/***************************************************************************
key.c  -  description
-------------------
begin                : Sun Oct 28 2001
copyright            : (C) 2001 by Pete Bernert
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

#define _IN_KEY

#include "externals.h"
#include "menu.h"
#include "gpu.h"
#include "draw.h"
#include "key.h"

void GPUmakeSnapshot(void);

unsigned long          ulKeybits=0;

void GPUkeypressed(int keycode)
{
}

void SetKeyHandler(void)
{
}

void ReleaseKeyHandler(void)
{
}
