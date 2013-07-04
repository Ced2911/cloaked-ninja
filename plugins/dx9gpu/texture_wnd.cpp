/***************************************************************************
texture.c  -  description
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

#include "stdafx.h"

////////////////////////////////////////////////////////////////////////////////////
// Texture related functions are here !
//
// The texture handling is heart and soul of this gpu. The plugin was developed
// 1999, by this time no shaders were available. Since the psx gpu is making
// heavy use of CLUT (="color lookup tables", aka palettized textures), it was 
// an interesting task to get those emulated at good speed on NV TNT cards 
// (which was my major goal when I created the first "gpuPeteTNT"). Later cards 
// (Geforce256) supported texture palettes by an OGL extension, but at some point
// this support was dropped again by gfx card vendors.
// Well, at least there is a certain advatage, if no texture palettes extension can
// be used: it is possible to modify the textures in any way, allowing "hi-res" 
// textures and other tweaks.
//
// My main texture caching is kinda complex: the plugin is allocating "n" 256x256 textures,
// and it places small psx texture parts inside them. The plugin keeps track what 
// part (with what palette) it had placed in which texture, so it can re-use this 
// part again. The more ogl textures it can use, the better (of course the managing/
// searching will be slower, but everything is faster than uploading textures again
// and again to a gfx card). My first card (TNT1) had 16 MB Vram, and it worked
// well with many games, but I recommend nowadays 64 MB Vram to get a good speed.
//
// Sadly, there is also a second kind of texture cache needed, for "psx texture windows".
// Those are "repeated" textures, so a psx "texture window" needs to be put in 
// a whole texture to use the GL_TEXTURE_WRAP_ features. This cache can get full very
// fast in games which are having an heavy "texture window" usage, like RRT4. As an 
// alternative, this plugin can use the OGL "palette" extension on texture windows, 
// if available. Nowadays also a fragment shader can easily be used to emulate
// texture wrapping in a texture atlas, so the main cache could hold the texture
// windows as well (that's what I am doing in the OGL2 plugin). But currently the
// OGL1 plugin is a "shader-free" zone, so heavy "texture window" games will cause
// much texture uploads.
//
// Some final advice: take care if you change things in here. I've removed my ASM
// handlers (they didn't cause much speed gain anyway) for readability/portability,
// but still the functions/data structures used here are easy to mess up. I guess it
// can be a pain in the ass to port the plugin to another byte order :)
//
////////////////////////////////////////////////////////////////////////////////////


#include "externals.h"
#include "backend.h"
#include "texture.h"
#include "gpu.h"
#include "prim.h"


static bk_texture_t * bktexture;
#define gTexName bktexture

#define MAXWNDTEXCACHE 128

static __inline uint32_t ptr32(void * addr){
    return GETLE32((uint32_t *)addr);
}

// "texture window" cache entry

#ifdef _WINDOWS
#pragma pack()
#endif

typedef struct textureWndCacheEntryTag
{
	uint32_t       ClutID;
	short          pageid;
	short          textureMode;
	short          Opaque;
	short          used;
	EXLong         pos;
	bk_texture_t * texname;
} textureWndCacheEntry;

#ifdef _WINDOWS
#pragma pack(1)
#endif

extern unsigned char ubPaletteBuffer[256][4];
extern unsigned char * texturepart;

extern unsigned short MAXTPAGES;
extern unsigned short CLUTMASK;
extern unsigned short CLUTYMASK;
extern unsigned short MAXSORTTEX;
extern uint32_t g_x1,g_y1,g_x2,g_y2;

textureWndCacheEntry wcWndtexStore[MAXWNDTEXCACHE];

int iMaxTexWnds = 0;
int iTexWndTurn = 0;
int iTexWndLimit = MAXWNDTEXCACHE/2;


void InitializeWndTextureStore() {
	iTexWndLimit=MAXWNDTEXCACHE/2;

	memset(wcWndtexStore,0,sizeof(textureWndCacheEntry)* MAXWNDTEXCACHE);
}

void CleanupTextureWndStore() 
{
	int i;
	textureWndCacheEntry * tsx;
	//----------------------------------------------------//
	tsx=wcWndtexStore;                                    // loop tex window cache
	for(i=0;i<MAXWNDTEXCACHE;i++,tsx++)
	{
		if(tsx->texname)                                    // -> some tex?
			BK_DeleteTexture(tsx->texname);                 // --> delete it
	}
	iMaxTexWnds=0;                                        // no more tex wnds
	//----------------------------------------------------//
}

void ResetTextureWndArea(BOOL bDelTex) {
	int i;
	textureWndCacheEntry * tsx;

	//----------------------------------------------------//
	tsx=wcWndtexStore;
	for(i=0;i<MAXWNDTEXCACHE;i++,tsx++)
	{
		tsx->used=0;
		if(bDelTex && tsx->texname)
		{
			BK_DeleteTexture(tsx->texname);
			tsx->texname=0;
		}
	}
	iMaxTexWnds=0;
}


////////////////////////////////////////////////////////////////////////
// Invalidate tex windows
////////////////////////////////////////////////////////////////////////

void InvalidateWndTextureArea(int X, int Y, int W, int H)
{
	int i,px1,px2,py1,py2,iYM=1;
	textureWndCacheEntry * tsw=wcWndtexStore;

	W+=X-1;      
	H+=Y-1;
	if(X<0) X=0;if(X>1023) X=1023;
	if(W<0) W=0;if(W>1023) W=1023;
	if(Y<0) Y=0;if(Y>iGPUHeightMask)  Y=iGPUHeightMask;
	if(H<0) H=0;if(H>iGPUHeightMask)  H=iGPUHeightMask;
	W++;H++;

	if(iGPUHeight==1024) iYM=3;

	py1=min(iYM,Y>>8);
	py2=min(iYM,H>>8);                                    // y: 0 or 1

	px1=max(0,(X>>6));
	px2=min(15,(W>>6));

	if(py1==py2)
	{
		py1=py1<<4;px1+=py1;px2+=py1;                       // change to 0-31
		for(i=0;i<iMaxTexWnds;i++,tsw++)
		{
			if(tsw->used)
			{
				if(tsw->pageid>=px1 && tsw->pageid<=px2)
				{
					tsw->used=0;
				}
			}
		}
	}
	else
	{
		py1=px1+16;py2=px2+16;
		for(i=0;i<iMaxTexWnds;i++,tsw++)
		{
			if(tsw->used)
			{
				if((tsw->pageid>=px1 && tsw->pageid<=px2) ||
					(tsw->pageid>=py1 && tsw->pageid<=py2))
				{
					tsw->used=0;
				}
			}
		}
	}

	// adjust tex window count
	tsw=wcWndtexStore+iMaxTexWnds-1;
	while(iMaxTexWnds && !tsw->used) {iMaxTexWnds--;tsw--;}
}

////////////////////////////////////////////////////////////////////////
// tex window: define
////////////////////////////////////////////////////////////////////////

int DefineTextureWnd(bk_texture_t * tex)
{	
	tex->wrap = WRAP_REPEAT;
	return BK_SetTextureData(tex, TWin.Position.x1, TWin.Position.x1, texturepart);
}


////////////////////////////////////////////////////////////////////////
// tex window: load stretched
////////////////////////////////////////////////////////////////////////
int LoadStretchWndTexturePage(int pageid, int mode, short cx, short cy, bk_texture_t * tex)
{
	uint32_t       start,row,column,j,sxh,sxm,ldx,ldy,ldxo,s;
	unsigned int   palstart;
	uint32_t       *px,*pa,*ta;
	unsigned char  *cSRCPtr,*cOSRCPtr;
	unsigned short *wSRCPtr,*wOSRCPtr;
	uint32_t       LineOffset;
	int            pmult = pageid / 16;
	uint32_t       (*LTCOL)(uint32_t);

	LTCOL = TCF[DrawSemiTrans];

	ldxo=TWin.Position.x1-TWin.OPosition.x1;
	ldy =TWin.Position.y1-TWin.OPosition.y1;

	pa = px = (uint32_t *)ubPaletteBuffer;
	ta = (uint32_t *)texturepart;
	palstart = cx + (cy * 1024);

	ubOpaqueDraw = 0;

	switch (mode)
	{
		//--------------------------------------------------// 
		// 4bit texture load ..
	case 0:
		//------------------- ZN STUFF

		if(GlobalTextIL)
		{
			unsigned int TXV,TXU,n_xi,n_yi;

			wSRCPtr=psxVuw+palstart;

			row=4;do
			{
				*px    =LTCOL(ptr32(wSRCPtr));
				*(px+1)=LTCOL(ptr32(wSRCPtr+1));
				*(px+2)=LTCOL(ptr32(wSRCPtr+2));
				*(px+3)=LTCOL(ptr32(wSRCPtr+3));
				row--;px+=4;wSRCPtr+=4;
			}
			while (row);

			column=g_y2-ldy;
			for(TXV=g_y1;TXV<=column;TXV++)
			{
				ldx=ldxo;
				for(TXU=g_x1;TXU<=g_x2-ldxo;TXU++)
				{
					n_xi = ( ( TXU >> 2 ) & ~0x3c ) + ( ( TXV << 2 ) & 0x3c );
					n_yi = ( TXV & ~0xf ) + ( ( TXU >> 4 ) & 0xf );

					s=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
					*ta++=s;

					if(ldx) {*ta++=s;ldx--;}
				}

				if(ldy) 
				{ldy--;
				for(TXU=g_x1;TXU<=g_x2;TXU++)
					*ta++=*(ta-(g_x2-g_x1));
				}
			}

			return DefineTextureWnd(tex);
		}

		//-------------------

		start=((pageid-16*pmult)*128)+256*2048*pmult;
		// convert CLUT to 32bits .. and then use THAT as a lookup table

		wSRCPtr=psxVuw+palstart;
		for(row=0;row<16;row++)
			*px++=LTCOL(ptr32(wSRCPtr++));

		sxm=g_x1&1;sxh=g_x1>>1;
		if(sxm) j=g_x1+1; else j=g_x1;
		cSRCPtr = psxVub + start + (2048*g_y1) + sxh;
		for(column=g_y1;column<=g_y2;column++)
		{
			cOSRCPtr=cSRCPtr;ldx=ldxo;
			if(sxm) *ta++=*(pa+((*cSRCPtr++ >> 4) & 0xF));

			for(row=j;row<=g_x2-ldxo;row++)
			{
				s=*(pa+(*cSRCPtr & 0xF));
				*ta++=s;
				if(ldx) {*ta++=s;ldx--;}
				row++;
				if(row<=g_x2-ldxo) 
				{
					s=*(pa+((*cSRCPtr >> 4) & 0xF));
					*ta++=s; 
					if(ldx) {*ta++=s;ldx--;}
				}
				cSRCPtr++;
			}
			if(ldy && column&1) 
			{ldy--;cSRCPtr = cOSRCPtr;}
			else cSRCPtr = psxVub + start + (2048*(column+1)) + sxh;
		}

		return DefineTextureWnd(tex);
		//--------------------------------------------------//
		// 8bit texture load ..
	case 1:
		//------------ ZN STUFF
		if(GlobalTextIL)
		{
			unsigned int TXV,TXU,n_xi,n_yi;

			wSRCPtr=psxVuw+palstart;

			row=64;do
			{
				*px    =LTCOL(ptr32(wSRCPtr));
				*(px+1)=LTCOL(ptr32(wSRCPtr+1));
				*(px+2)=LTCOL(ptr32(wSRCPtr+2));
				*(px+3)=LTCOL(ptr32(wSRCPtr+3));
				row--;px+=4;wSRCPtr+=4;
			}
			while (row);

			column=g_y2-ldy;
			for(TXV=g_y1;TXV<=column;TXV++)
			{
				ldx=ldxo;
				for(TXU=g_x1;TXU<=g_x2-ldxo;TXU++)
				{
					n_xi = ( ( TXU >> 1 ) & ~0x78 ) + ( ( TXU << 2 ) & 0x40 ) + ( ( TXV << 3 ) & 0x38 );
					n_yi = ( TXV & ~0x7 ) + ( ( TXU >> 5 ) & 0x7 );

					s=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff));
					*ta++=s;
					if(ldx) {*ta++=s;ldx--;}
				}

				if(ldy) 
				{ldy--;
				for(TXU=g_x1;TXU<=g_x2;TXU++)
					*ta++=*(ta-(g_x2-g_x1));
				}

			}

			return DefineTextureWnd(tex);
		}
		//------------

		start=((pageid-16*pmult)*128)+256*2048*pmult;    

		// not using a lookup table here... speeds up smaller texture areas
		cSRCPtr = psxVub + start + (2048*g_y1) + g_x1;
		LineOffset = 2048 - (g_x2-g_x1+1) +ldxo; 

		for(column=g_y1;column<=g_y2;column++)
		{
			cOSRCPtr=cSRCPtr;ldx=ldxo;
			for(row=g_x1;row<=g_x2-ldxo;row++)
			{
				s=LTCOL(ptr32(&psxVuw[palstart+ *cSRCPtr++]));
				*ta++=s;
				if(ldx) {*ta++=s;ldx--;}
			}
			if(ldy && column&1) {ldy--;cSRCPtr=cOSRCPtr;}
			else                cSRCPtr+=LineOffset;
		}

		return DefineTextureWnd(tex);
		//--------------------------------------------------// 
		// 16bit texture load ..
	case 2:
		start=((pageid-16*pmult)*64)+256*1024*pmult;

		wSRCPtr = psxVuw + start + (1024*g_y1) + g_x1;
		LineOffset = 1024 - (g_x2-g_x1+1) +ldxo; 

		for(column=g_y1;column<=g_y2;column++)
		{
			wOSRCPtr=wSRCPtr;ldx=ldxo;
			for(row=g_x1;row<=g_x2-ldxo;row++)
			{
				s=LTCOL(ptr32(wSRCPtr++));
				*ta++=s;
				if(ldx) {*ta++=s;ldx--;}
			}
			if(ldy && column&1) {ldy--;wSRCPtr=wOSRCPtr;}
			else                 wSRCPtr+=LineOffset;
		}

		return DefineTextureWnd(tex);
		//--------------------------------------------------// 
		// others are not possible !
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////
// tex window: load simple
////////////////////////////////////////////////////////////////////////

int LoadWndTexturePage(int pageid, int mode, short cx, short cy, bk_texture_t * tex)
{
	uint32_t       start,row,column,j,sxh,sxm;
	unsigned int   palstart;
	uint32_t       *px,*pa,*ta;
	unsigned char  *cSRCPtr;
	unsigned short *wSRCPtr;
	uint32_t        LineOffset;
	int pmult = pageid / 16;
	uint32_t (*LTCOL)(uint32_t);

	LTCOL=TCF[DrawSemiTrans];

	pa = px = (uint32_t *)ubPaletteBuffer;
	ta = (uint32_t *)texturepart;
	palstart = cx + (cy * 1024);

	ubOpaqueDraw = 0;

	switch (mode)
	{
		//--------------------------------------------------// 
		// 4bit texture load ..
	case 0:
		if(GlobalTextIL)
		{
			unsigned int TXV,TXU,n_xi,n_yi;

			wSRCPtr=psxVuw+palstart;

			row=4;do
			{
				*px    =LTCOL(ptr32(wSRCPtr));
				*(px+1)=LTCOL(ptr32(wSRCPtr+1));
				*(px+2)=LTCOL(ptr32(wSRCPtr+2));
				*(px+3)=LTCOL(ptr32(wSRCPtr+3));
				row--;px+=4;wSRCPtr+=4;
			}
			while (row);

			for(TXV=g_y1;TXV<=g_y2;TXV++)
			{
				for(TXU=g_x1;TXU<=g_x2;TXU++)
				{
					n_xi = ( ( TXU >> 2 ) & ~0x3c ) + ( ( TXV << 2 ) & 0x3c );
					n_yi = ( TXV & ~0xf ) + ( ( TXU >> 4 ) & 0xf );

					*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
				}
			}

			return DefineTextureWnd(tex);
		}

		start=((pageid-16*pmult)*128)+256*2048*pmult;

		// convert CLUT to 32bits .. and then use THAT as a lookup table

		wSRCPtr=psxVuw+palstart;
		for(row=0;row<16;row++)
			*px++=LTCOL(ptr32(wSRCPtr++));

		sxm=g_x1&1;sxh=g_x1>>1;
		if(sxm) j=g_x1+1; else j=g_x1;
		cSRCPtr = psxVub + start + (2048*g_y1) + sxh;
		for(column=g_y1;column<=g_y2;column++)
		{
			cSRCPtr = psxVub + start + (2048*column) + sxh;

			if(sxm) *ta++=*(pa+((*cSRCPtr++ >> 4) & 0xF));

			for(row=j;row<=g_x2;row++)
			{
				*ta++=*(pa+(*cSRCPtr & 0xF)); row++;
				if(row<=g_x2) *ta++=*(pa+((*cSRCPtr >> 4) & 0xF)); 
				cSRCPtr++;
			}
		}

		return DefineTextureWnd(tex);
		//--------------------------------------------------//
		// 8bit texture load ..
	case 1:
		if(GlobalTextIL)
		{
			unsigned int TXV,TXU,n_xi,n_yi;

			wSRCPtr=psxVuw+palstart;

			row=64;do
			{
				*px    =LTCOL(ptr32(wSRCPtr));
				*(px+1)=LTCOL(ptr32(wSRCPtr+1));
				*(px+2)=LTCOL(ptr32(wSRCPtr+2));
				*(px+3)=LTCOL(ptr32(wSRCPtr+3));
				row--;px+=4;wSRCPtr+=4;
			}
			while (row);

			for(TXV=g_y1;TXV<=g_y2;TXV++)
			{
				for(TXU=g_x1;TXU<=g_x2;TXU++)
				{
					n_xi = ( ( TXU >> 1 ) & ~0x78 ) + ( ( TXU << 2 ) & 0x40 ) + ( ( TXV << 3 ) & 0x38 );
					n_yi = ( TXV & ~0x7 ) + ( ( TXU >> 5 ) & 0x7 );

					*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff));
				}
			}

			return DefineTextureWnd(tex);
		}

		start=((pageid-16*pmult)*128)+256*2048*pmult;

		// not using a lookup table here... speeds up smaller texture areas
		cSRCPtr = psxVub + start + (2048*g_y1) + g_x1;
		LineOffset = 2048 - (g_x2-g_x1+1); 

		for(column=g_y1;column<=g_y2;column++)
		{
			for(row=g_x1;row<=g_x2;row++)
				*ta++=LTCOL(ptr32(&psxVuw[palstart+ *cSRCPtr++]));
			cSRCPtr+=LineOffset;
		}

		return DefineTextureWnd(tex);
		//--------------------------------------------------// 
		// 16bit texture load ..
	case 2:
		start=((pageid-16*pmult)*64)+256*1024*pmult;

		wSRCPtr = psxVuw + start + (1024*g_y1) + g_x1;
		LineOffset = 1024 - (g_x2-g_x1+1); 

		for(column=g_y1;column<=g_y2;column++)
		{
			for(row=g_x1;row<=g_x2;row++)
				*ta++=LTCOL(ptr32(wSRCPtr++));
			wSRCPtr+=LineOffset;
		}

		return DefineTextureWnd(tex);
		//--------------------------------------------------// 
		// others are not possible !
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////
// tex window: main selecting, cache handler included
////////////////////////////////////////////////////////////////////////

bk_texture_t * LoadTextureWnd(int pageid, int TextureMode, uint32_t GivenClutId)
{
	bk_texture_t * tex = NULL;
	textureWndCacheEntry *ts, *tsx = NULL;
	int i;
	short cx,cy;
	EXLong npos;

	npos.c[3] = TWin.Position.x0;
	npos.c[2] = TWin.OPosition.x1;
	npos.c[1] = TWin.Position.y0;
	npos.c[0] = TWin.OPosition.y1;

	g_x1 = TWin.Position.x0; g_x2 = g_x1 + TWin.Position.x1 - 1;
	g_y1 = TWin.Position.y0; g_y2 = g_y1 + TWin.Position.y1 - 1;

	if (TextureMode == 2) { GivenClutId = 0; cx = cy = 0; }
	else  
	{
		cx = ((GivenClutId << 4) & 0x3F0);
		cy = ((GivenClutId >> 6) & CLUTYMASK);
		GivenClutId = (GivenClutId & CLUTMASK) | (DrawSemiTrans << 30);

		// palette check sum
		{
			uint32_t l = 0,row;
			uint32_t *lSRCPtr = (uint32_t *)(psxVuw + cx + (cy * 1024));
			if(TextureMode==1) for(row=1;row<129;row++) l+=((*lSRCPtr++)-1)*row;
			else               for(row=1;row<9;row++)   l+=((*lSRCPtr++)-1)<<row;
			l=(l+HIWORD(l))&0x3fffL;
			GivenClutId|=(l<<16);
		}

	}

	ts=wcWndtexStore;

	for(i=0;i<iMaxTexWnds;i++,ts++)
	{
		if(ts->used)
		{
			if(ts->pos.l==npos.l &&
				ts->pageid==pageid &&
				ts->textureMode==TextureMode)
			{
				if(ts->ClutID==GivenClutId)
				{
					ubOpaqueDraw=ts->Opaque;
					//return bktexture;
					return ts->texname;
				}
			}
		}
		else tsx=ts;
	}

	if(!tsx) 
	{
		if(iMaxTexWnds==iTexWndLimit)
		{
			tsx=wcWndtexStore+iTexWndTurn;
			iTexWndTurn++; 
			if(iTexWndTurn==iTexWndLimit) iTexWndTurn=0;
		}
		else
		{
			tsx=wcWndtexStore+iMaxTexWnds;
			iMaxTexWnds++;
		}
	}

	tex = tsx->texname;

	if (tex == NULL) {
		tex = BK_CreateTexture(TWin.Position.x1, TWin.Position.x1, 0);
	}

	if(TWin.OPosition.y1==TWin.Position.y1 &&
		TWin.OPosition.x1==TWin.Position.x1)
	{
		LoadWndTexturePage(pageid, TextureMode, cx, cy, tex);
	}       
	else
	{
		LoadStretchWndTexturePage(pageid, TextureMode, cx, cy, tex);
	}

	tsx->Opaque=ubOpaqueDraw;
	tsx->pos.l=npos.l;
	tsx->ClutID=GivenClutId;
	tsx->pageid=pageid;
	tsx->textureMode=TextureMode;
	tsx->texname=tex;
	tsx->used=1;

	return tex;
}