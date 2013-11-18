/***************************************************************************
                         soft.h  -  description
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

#ifndef _GPU_SOFT_H_
#define _GPU_SOFT_H_

void offsetPSXLine(void);
void offsetPSX2(void);
void offsetPSX3(void);
void offsetPSX4(void);

void FillSoftwareAreaTrans(short x0,short y0,short x1,short y1,unsigned short col);
void FillSoftwareArea(short x0,short y0,short x1,short y1,unsigned short col);
void drawPoly3G(int32_t rgb1, int32_t rgb2, int32_t rgb3);
void drawPoly4G(int32_t rgb1, int32_t rgb2, int32_t rgb3, int32_t rgb4);
void drawPoly3F(int32_t rgb);
void drawPoly4F(int32_t rgb);
void drawPoly4FT(unsigned char * baseAddr);
void drawPoly4GT(unsigned char * baseAddr);
void drawPoly3FT(unsigned char * baseAddr);
void drawPoly3GT(unsigned char * baseAddr);
void DrawSoftwareSprite(unsigned char * baseAddr,short w,short h,int32_t tx,int32_t ty);
void DrawSoftwareSpriteTWin(unsigned char * baseAddr,int32_t w,int32_t h);
void DrawSoftwareSpriteMirror(unsigned char * baseAddr,int32_t w,int32_t h);
void DrawSoftwareLineShade(int32_t rgb0, int32_t rgb1);
void DrawSoftwareLineFlat(int32_t rgb);



////////////////////////////////////////////////////////////////////////////////////
// defines
////////////////////////////////////////////////////////////////////////////////////

// switches for painting textured quads as 2 triangles (small glitches, but better shading!)
// can be toggled by game fix 0x200 in version 1.17 anyway, so let the defines enabled!

#define POLYQUAD3                 
#define POLYQUAD3GT                 

// fast solid loops... a bit more additional code, of course

#define FASTSOLID

// psx blending mode 3 with 25% incoming color (instead 50% without the define)

#define HALFBRIGHTMODE3

// color decode defines

#define XCOL1(x)     (x & 0x1f)
#define XCOL2(x)     (x & 0x3e0)
#define XCOL3(x)     (x & 0x7c00)

#define XCOL1D(x)     (x & 0x1f)
#define XCOL2D(x)     ((x>>5) & 0x1f)
#define XCOL3D(x)     ((x>>10) & 0x1f)

#define X32TCOL1(x)  ((x & 0x001f001f)<<7)
#define X32TCOL2(x)  ((x & 0x03e003e0)<<2)
#define X32TCOL3(x)  ((x & 0x7c007c00)>>3)

#define X32COL1(x)   (x & 0x001f001f)
#define X32COL2(x)   ((x>>5) & 0x001f001f)
#define X32COL3(x)   ((x>>10) & 0x001f001f)

#define X32ACOL1(x)  (x & 0x001e001e)
#define X32ACOL2(x)  ((x>>5) & 0x001e001e)
#define X32ACOL3(x)  ((x>>10) & 0x001e001e)

#define X32BCOL1(x)  (x & 0x001c001c)
#define X32BCOL2(x)  ((x>>5) & 0x001c001c)
#define X32BCOL3(x)  ((x>>10) & 0x001c001c)

#define X32PSXCOL(r,g,b) ((g<<10)|(b<<5)|r)

#define XPSXCOL(r,g,b) ((g&0x7c00)|(b&0x3e0)|(r&0x1f))

#endif // _GPU_SOFT_H_
