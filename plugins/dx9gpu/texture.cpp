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

#define _IN_TEXTURE

#include "externals.h"
#include "backend.h"
#include "texture.h"
#include "gpu.h"
#include "prim.h"

static __inline uint32_t ptr32(void * addr){
    return GETLE32((uint32_t *)addr);
}

#define USE_BK

#define CLUTCHK   0x00060000
#define CLUTSHIFT 17

////////////////////////////////////////////////////////////////////////
// texture conversion buffer .. 
////////////////////////////////////////////////////////////////////////

int           iHiResTextures=0;
unsigned char       ubPaletteBuffer[256][4];

int           iTexGarbageCollection=1;
uint32_t      dwTexPageComp=0;
int           iVRamSize=0;

void               (*LoadSubTexFn) (int,int,short,short);
uint32_t           (*PalTexturedColourFn)  (uint32_t);

////////////////////////////////////////////////////////////////////////
// defines
////////////////////////////////////////////////////////////////////////

#define PALCOL(x) PalTexturedColourFn (x)

#define CSUBSIZE  2048
#define CSUBSIZEA 8192
#define CSUBSIZES 4096

#define OFFA 0
#define OFFB 2048
#define OFFC 4096
#define OFFD 6144

#define XOFFA 0
#define XOFFB 512
#define XOFFC 1024
#define XOFFD 1536

#define SOFFA 0
#define SOFFB 1024
#define SOFFC 2048
#define SOFFD 3072

#define XCHECK(pos1,pos2) ((pos1._0>=pos2._1)&&(pos1._1<=pos2._0)&&(pos1._2>=pos2._3)&&(pos1._3<=pos2._2))
#define INCHECK(pos2,pos1) ((pos1._0<=pos2._0) && (pos1._1>=pos2._1) && (pos1._2<=pos2._2) && (pos1._3>=pos2._3))

////////////////////////////////////////////////////////////////////////

unsigned char * CheckTextureInSubSCache(int TextureMode, uint32_t GivenClutId, unsigned short *pCache);
void            LoadSubTexturePageSort(int pageid, int mode, short cx, short cy);
void            LoadPackedSubTexturePageSort(int pageid, int mode, short cx, short cy);

////////////////////////////////////////////////////////////////////////
// some globals
////////////////////////////////////////////////////////////////////////
int   GlobalTexturePage;
int XTexS;
int YTexS;
int DXTexS;
int DYTexS;
int   iSortTexCnt=32;
BOOL  bUseFastMdec=FALSE;
BOOL  bUse15bitMdec=FALSE;
int   iFrameTexType=0;
int   iFrameReadType=0;

uint32_t       (*TCF[2]) (uint32_t);
unsigned short (*PTCF[2]) (unsigned short);

////////////////////////////////////////////////////////////////////////
// texture cache implementation
////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS
#pragma pack(1)
#endif


// "standard texture" cache entry (12 byte per entry, as small as possible... we need lots of them)

typedef struct textureSubCacheEntryTagS 
{
	uint32_t        ClutID;
	EXLong          pos;
	unsigned char   posTX;
	unsigned char   posTY;
	unsigned char   cTexID;
	unsigned char   Opaque;
} textureSubCacheEntryS;

#ifdef _WINDOWS
#pragma pack()
#endif

//---------------------------------------------

#define MAXTPAGES_MAX  64
#define MAXSORTTEX_MAX 196

//---------------------------------------------

textureSubCacheEntryS *  pscSubtexStore[3][MAXTPAGES_MAX];
EXLong *                 pxSsubtexLeft [MAXSORTTEX_MAX];
bk_texture_t *           uiStexturePage[MAXSORTTEX_MAX];

unsigned short           usLRUTexPage = 0;

unsigned char *                texturepart = NULL;
unsigned char *                texturebuffer = NULL;
uint32_t                 g_x1,g_y1,g_x2,g_y2;
unsigned char            ubOpaqueDraw = 0;

unsigned short MAXTPAGES     = 32;
unsigned short CLUTMASK      = 0x7fff;
unsigned short CLUTYMASK     = 0x1ff;
unsigned short MAXSORTTEX    = 196;

////////////////////////////////////////////////////////////////////////
// Texture color conversions... all my ASM funcs are removed for easier
// porting... and honestly: nowadays the speed gain would be pointless 
////////////////////////////////////////////////////////////////////////

uint32_t XP8RGBA(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x50000000;
	if(DrawSemiTrans && !(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff);}
	return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t XP8RGBAEx(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x03000000;
	if(DrawSemiTrans && !(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff);}
	return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t CP8RGBA(uint32_t BGR)
{
	uint32_t l;
	if(!(BGR&0xffff)) return 0x50000000;
	if(DrawSemiTrans && !(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff);}
	l=((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
	if(l==0xffffff00) l=0xff000000;
	return l;
}

uint32_t CP8RGBAEx(uint32_t BGR)
{
	uint32_t l;
	if(!(BGR&0xffff)) return 0x03000000;
	if(DrawSemiTrans && !(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff);}
	l=((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
	if(l==0xffffff00) l=0xff000000;
	return l;
}

uint32_t XP8RGBA_0(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x50000000;
	return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t XP8RGBAEx_0(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x03000000;
	return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t XP8BGRA_0(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x50000000;
	return ((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t XP8BGRAEx_0(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x03000000;
	return ((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t CP8RGBA_0(uint32_t BGR)
{
	uint32_t l;

	if(!(BGR&0xffff)) return 0x50000000;
	l=((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
	if(l==0xfff8f800) l=0xff000000;
	return l;
}

uint32_t CP8RGBAEx_0(uint32_t BGR)
{
	uint32_t l;

	if(!(BGR&0xffff)) return 0x03000000;
	l=((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
	if(l==0xfff8f800) l=0xff000000;
	return l;
}

uint32_t CP8BGRA_0(uint32_t BGR)
{
	uint32_t l;

	if(!(BGR&0xffff)) return 0x50000000;
	l=((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff)|0xff000000;
	if(l==0xff00f8f8) l=0xff000000;
	return l;
}

uint32_t CP8BGRAEx_0(uint32_t BGR)
{
	uint32_t l;

	if(!(BGR&0xffff)) return 0x03000000;
	l=((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff)|0xff000000;
	if(l==0xff00f8f8) l=0xff000000;
	return l;
}

uint32_t XP8RGBA_1(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x50000000;
	if(!(BGR&0x8000)) {ubOpaqueDraw=1;return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff);}
	return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t XP8RGBAEx_1(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x03000000;
	if(!(BGR&0x8000)) {ubOpaqueDraw=1;return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff);}
	return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t XP8BGRA_1(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x50000000;
	if(!(BGR&0x8000)) {ubOpaqueDraw=1;return ((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff);}
	return ((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t XP8BGRAEx_1(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0x03000000;
	if(!(BGR&0x8000)) {ubOpaqueDraw=1;return ((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff);}
	return ((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t P8RGBA(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0;
	return ((((BGR<<3)&0xf8)|((BGR<<6)&0xf800)|((BGR<<9)&0xf80000))&0xffffff)|0xff000000;
}

uint32_t P8BGRA(uint32_t BGR)
{
	if(!(BGR&0xffff)) return 0;
	return ((((BGR>>7)&0xf8)|((BGR<<6)&0xf800)|((BGR<<19)&0xf80000))&0xffffff)|0xff000000;
}

unsigned short XP5RGBA(unsigned short BGR)
{
	if(!BGR) return 0;
	if(DrawSemiTrans && !(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)));}
	return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)))|1;
}

unsigned short XP5RGBA_0 (unsigned short BGR)
{
	if(!BGR) return 0;

	return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)))|1;
}

unsigned short CP5RGBA_0 (unsigned short BGR)
{
	unsigned short s;

	if(!BGR) return 0;

	s=((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)))|1;
	if(s==0x07ff) s=1;
	return s;
}

unsigned short XP5RGBA_1(unsigned short BGR)
{
	if(!BGR) return 0;
	if(!(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)));}
	return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)))|1;
}

unsigned short P5RGBA(unsigned short BGR)
{
	if(!BGR) return 0;
	return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)))|1;
}

unsigned short XP4RGBA(unsigned short BGR)
{
	if(!BGR) return 6;
	if(DrawSemiTrans && !(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)));}
	return (((((BGR&0x1e)<<11))|((BGR&0x7800)>>7)|((BGR&0x3c0)<<2)))|0xf;
}

unsigned short XP4RGBA_0 (unsigned short BGR)
{
	if(!BGR) return 6;
	return (((((BGR&0x1e)<<11))|((BGR&0x7800)>>7)|((BGR&0x3c0)<<2)))|0xf;
}

unsigned short CP4RGBA_0 (unsigned short BGR)
{
	unsigned short s;
	if(!BGR) return 6;
	s=(((((BGR&0x1e)<<11))|((BGR&0x7800)>>7)|((BGR&0x3c0)<<2)))|0xf;
	if(s==0x0fff) s=0x000f;
	return s;
}

unsigned short XP4RGBA_1(unsigned short BGR)
{
	if(!BGR) return 6;
	if(!(BGR&0x8000)) 
	{ubOpaqueDraw=1;return ((((BGR<<11))|((BGR>>9)&0x3e)|((BGR<<1)&0x7c0)));}
	return (((((BGR&0x1e)<<11))|((BGR&0x7800)>>7)|((BGR&0x3c0)<<2)))|0xf;
}

unsigned short P4RGBA(unsigned short BGR)
{
	if(!BGR) return 0;
	return (((((BGR&0x1e)<<11))|((BGR&0x7800)>>7)|((BGR&0x3c0)<<2)))|0xf;
}

////////////////////////////////////////////////////////////////////////
// CHECK TEXTURE MEM (on plugin startup)
////////////////////////////////////////////////////////////////////////

int iFTexA=512;
int iFTexB=512;

void CheckTextureMemory(void) {
	iSortTexCnt=8;
	iFTexA=1024;
	iFTexB=2048;
}

////////////////////////////////////////////////////////////////////////
// Main init of textures
////////////////////////////////////////////////////////////////////////
extern void InitializeWndTextureStore();
void InitializeTextureStore() 
{
	int i,j;

	if(iGPUHeight==1024)
	{
		MAXTPAGES     = 64;
		CLUTMASK      = 0xffff;
		CLUTYMASK     = 0x3ff;
		MAXSORTTEX    = 128;
		iTexGarbageCollection=0;
	}
	else
	{
		MAXTPAGES     = 32;
		CLUTMASK      = 0x7fff;
		CLUTYMASK     = 0x1ff;
		MAXSORTTEX    = 196;
	}

	memset(vertex,0,4*sizeof(OGLVertex));                 // init vertices

	gTexName=0;                                           // init main tex name

	InitializeWndTextureStore();

	texturepart=(unsigned char *)malloc(256*256*4);
	memset(texturepart,0,256*256*4);
	if(iHiResTextures)
		texturebuffer=(unsigned char *)malloc(512*512*4);
	else texturebuffer=NULL;

	for(i=0;i<3;i++)                                    // -> info for 32*3
		for(j=0;j<MAXTPAGES;j++)
		{                                               
			pscSubtexStore[i][j]=(textureSubCacheEntryS *)malloc(CSUBSIZES*sizeof(textureSubCacheEntryS));
			memset(pscSubtexStore[i][j],0,CSUBSIZES*sizeof(textureSubCacheEntryS));
		}
		for(i=0;i<MAXSORTTEX;i++)                           // -> info 0..511
		{
			pxSsubtexLeft[i]=(EXLong *)malloc(CSUBSIZE*sizeof(EXLong));
			memset(pxSsubtexLeft[i],0,CSUBSIZE*sizeof(EXLong));
			uiStexturePage[i]=0;
		}
}

////////////////////////////////////////////////////////////////////////
// Clean up on exit
////////////////////////////////////////////////////////////////////////
extern void CleanupTextureWndStore();
void CleanupTextureStore() 
{
	CleanupTextureWndStore();
	//----------------------------------------------------//
	for(int i=0;i<3;i++)                                    // -> loop
	{
		for(int j=0;j<MAXTPAGES;j++)                           // loop tex pages
		{
			free(pscSubtexStore[i][j]);                      // -> clean mem
		}
	}
	for(int j=0;j<MAXSORTTEX;j++)
	{
		if(uiStexturePage[j])                             // --> tex used ?
		{
			BK_DeleteTexture(uiStexturePage[j]);
			uiStexturePage[j]=0;                            // --> delete it
		}
		free(pxSsubtexLeft[j]);                           // -> clean mem
	}
	//----------------------------------------------------//
}

////////////////////////////////////////////////////////////////////////
// Reset textures in game...
////////////////////////////////////////////////////////////////////////
extern void ResetTextureWndArea(BOOL bDelTex);
void ResetTextureArea(BOOL bDelTex)
{
	int i,j;textureSubCacheEntryS * tss;EXLong * lu;
	//----------------------------------------------------//

	dwTexPageComp=0;

	//----------------------------------------------------//
	if(bDelTex) {
		BK_TextureDisable();
		gTexName=0;
	}
	//----------------------------------------------------//
	ResetTextureWndArea(bDelTex);
	//----------------------------------------------------//

	for(i=0;i<3;i++)
		for(j=0;j<MAXTPAGES;j++)
		{
			tss=pscSubtexStore[i][j];
			(tss+SOFFA)->pos.l=0;
			(tss+SOFFB)->pos.l=0;
			(tss+SOFFC)->pos.l=0;
			(tss+SOFFD)->pos.l=0;
		}

		for(i=0;i<iSortTexCnt;i++)
		{
			lu=pxSsubtexLeft[i];
			lu->l=0;
			if(bDelTex && uiStexturePage[i])
			{
				BK_DeleteTexture(uiStexturePage[i]);
				uiStexturePage[i]=0;
			}
		}
}

////////////////////////////////////////////////////////////////////////
// same for sort textures
////////////////////////////////////////////////////////////////////////

void MarkFree(textureSubCacheEntryS * tsx)
{
	EXLong * ul, * uls;
	int j,iMax;unsigned char x1,y1,dx,dy;

	uls=pxSsubtexLeft[tsx->cTexID];
	iMax=uls->l;ul=uls+1;

	if(!iMax) return;

	for(j=0;j<iMax;j++,ul++)
		if(ul->l==0xffffffff) break;

	if(j<CSUBSIZE-2)
	{
		if(j==iMax) uls->l=uls->l+1;

		x1=tsx->posTX;dx=tsx->pos._2-tsx->pos._3;
		if(tsx->posTX) {x1--;dx+=3;}
		y1=tsx->posTY;dy=tsx->pos._0-tsx->pos._1;
		if(tsx->posTY) {y1--;dy+=3;}

		ul->_3=x1;
		ul->_2=dx;
		ul->_1=y1;
		ul->_0=dy;
	}
}

void InvalidateSubSTextureArea(int X, int Y, int W, int H)
{
	int i,j,k,iMax,px,py,px1,px2,py1,py2,iYM = 1;
	EXLong npos;
	textureSubCacheEntryS *tsb;
	int x1,x2,y1,y2,xa,sw;

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
	px1=max(0,(X>>6)-3);                                   
	px2=min(15,(W>>6)+3);                                 // x: 0-15

	for(py=py1;py<=py2;py++)
	{
		j=(py<<4)+px1;                                      // get page

		y1=py*256;y2=y1+255;

		if(H<y1)  continue;
		if(Y>y2)  continue;

		if(Y>y1)  y1=Y;
		if(H<y2)  y2=H;
		if(y2<y1) {sw=y1;y1=y2;y2=sw;}
		y1=((y1%256)<<8);
		y2=(y2%256);

		for(px=px1;px<=px2;px++,j++)
		{
			for(k=0;k<3;k++)
			{
				xa=x1=px<<6;
				if(W<x1) continue;
				x2=x1+(64<<k)-1;
				if(X>x2) continue;

				if(X>x1)  x1=X;
				if(W<x2)  x2=W;
				if(x2<x1) {sw=x1;x1=x2;x2=sw;}

				if (dwGPUVersion == 2)
					npos.l=0x00ff00ff;
				else
					npos.l=((x1-xa)<<(26-k))|((x2-xa)<<(18-k))|y1|y2;

				{
					tsb=pscSubtexStore[k][j]+SOFFA;iMax=tsb->pos.l;tsb++;
					for(i=0;i<iMax;i++,tsb++)
						if(tsb->ClutID && XCHECK(tsb->pos,npos)) {tsb->ClutID=0;MarkFree(tsb);}

						//         if(npos.l & 0x00800000)
						{
							tsb=pscSubtexStore[k][j]+SOFFB;iMax=tsb->pos.l;tsb++;
							for(i=0;i<iMax;i++,tsb++)
								if(tsb->ClutID && XCHECK(tsb->pos,npos)) {tsb->ClutID=0;MarkFree(tsb);}
						}

						//         if(npos.l & 0x00000080)
						{
							tsb=pscSubtexStore[k][j]+SOFFC;iMax=tsb->pos.l;tsb++;
							for(i=0;i<iMax;i++,tsb++)
								if(tsb->ClutID && XCHECK(tsb->pos,npos)) {tsb->ClutID=0;MarkFree(tsb);}
						}

						//         if(npos.l & 0x00800080)
						{
							tsb=pscSubtexStore[k][j]+SOFFD;iMax=tsb->pos.l;tsb++;
							for(i=0;i<iMax;i++,tsb++)
								if(tsb->ClutID && XCHECK(tsb->pos,npos)) {tsb->ClutID=0;MarkFree(tsb);}
						}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////
// Invalidate some parts of cache: main routine
////////////////////////////////////////////////////////////////////////
extern void InvalidateWndTextureArea(int X, int Y, int W, int H);
extern int iMaxTexWnds;

void InvalidateTextureAreaEx(void)
{
	short W=sxmax-sxmin;
	short H=symax-symin;

	if (W == 0 && H == 0) return;

	if (iMaxTexWnds) 
		InvalidateWndTextureArea(sxmin,symin,W,H);

	InvalidateSubSTextureArea(sxmin,symin,W,H);
}

////////////////////////////////////////////////////////////////////////

void InvalidateTextureArea(int X, int Y, int W, int H)
{
	if (W == 0 && H == 0) return;

	if (iMaxTexWnds) InvalidateWndTextureArea(X, Y, W, H); 

	InvalidateSubSTextureArea(X, Y, W, H);
}

/////////////////////////////////////////////////////////////////////////////

BOOL bFakeFrontBuffer=FALSE;
BOOL bIgnoreNextTile =FALSE;

int iFTex=512;

bk_texture_t * FakeFramebuffer(void) {
	bk_texture_t * gpu_fb = NULL;
	int pmult;short x1,x2,y1,y2;int iYAdjust;
	float ScaleX,ScaleY;RECT rSrc;
	if(PSXDisplay.InterlacedTest) return 0;
	pmult=GlobalTexturePage/16;
	x1=gl_ux[7];
	x2=gl_ux[6]-gl_ux[7];
	y1=gl_ux[5];
	y2=gl_ux[4]-gl_ux[5];

	y1+=pmult*256;
	x1+=((GlobalTexturePage-16*pmult)<<6);
	if(!FastCheckAgainstFrontScreen(x1,y1,x2,y2) &&
		!FastCheckAgainstScreen(x1,y1,x2,y2))
		return 0;

	if(bFakeFrontBuffer) bIgnoreNextTile=TRUE;
	CheckVRamReadEx(x1,y1,x1+x2,y1+y2);
	return 0;
}

bk_texture_t * Fake15BitTexture(void)
{
	bk_texture_t * gpu_fb = NULL;
//#ifndef USE_BK
#if 1
	int pmult;short x1,x2,y1,y2;int iYAdjust;
	float ScaleX,ScaleY;RECT rSrc;
#if 0
	if(iFrameTexType==1) return BlackFake15BitTexture();
	if(PSXDisplay.InterlacedTest) return 0;
#endif
	pmult=GlobalTexturePage/16;
	x1=gl_ux[7];
	x2=gl_ux[6]-gl_ux[7];
	y1=gl_ux[5];
	y2=gl_ux[4]-gl_ux[5];

	y1+=pmult*256;
	x1+=((GlobalTexturePage-16*pmult)<<6);
#if 0
	if(iFrameTexType==3)
	{
		if(iFrameReadType==4) return 0;

		if(!FastCheckAgainstFrontScreen(x1,y1,x2,y2) &&
			!FastCheckAgainstScreen(x1,y1,x2,y2))
			return 0;

		if(bFakeFrontBuffer) bIgnoreNextTile=TRUE;
		CheckVRamReadEx(x1,y1,x1+x2,y1+y2);
		return 0;
	}
#endif
	/////////////////////////

	if(FastCheckAgainstFrontScreen(x1,y1,x2,y2))
	{
		x1-=PSXDisplay.DisplayPosition.x;
		y1-=PSXDisplay.DisplayPosition.y;
	}
	else if(FastCheckAgainstScreen(x1,y1,x2,y2))
	{
		x1-=PreviousPSXDisplay.DisplayPosition.x;
		y1-=PreviousPSXDisplay.DisplayPosition.y;
	}
	else {
		return 0;
	}
#if 0
		if(!gTexFrameName)
		{
			char * p;

			iFTex=2048;

			glGenTextures(1, &gTexFrameName);
			gTexName=gTexFrameName;
			glBindTexture(GL_TEXTURE_2D, gTexName);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iClampType);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, iClampType);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			p=(char *)malloc(iFTex*iFTex*4);
			memset(p,0,iFTex*iFTex*4);
			glTexImage2D(GL_TEXTURE_2D, 0, 3, iFTex, iFTex, 0, GL_RGB, GL_UNSIGNED_BYTE, p);
			free(p);

			glGetError();
		}
		else 
		{
			gTexName=gTexFrameName;
			glBindTexture(GL_TEXTURE_2D, gTexName);
		}
#endif
		x1+=PreviousPSXDisplay.Range.x0;
		y1+=PreviousPSXDisplay.Range.y0;

		if(PSXDisplay.DisplayMode.x)
			ScaleX=(float)iResX/(float)PSXDisplay.DisplayMode.x;
		else ScaleX=1.0f;
		if(PSXDisplay.DisplayMode.y)
			ScaleY=(float)iResY/(float)PSXDisplay.DisplayMode.y;
		else ScaleY=1.0f;

		rSrc.left  =max(x1*ScaleX,0);
		rSrc.right =min((x1+x2)*ScaleX+0.99f,iResX-1);
		rSrc.top   =max(y1*ScaleY,0);
		rSrc.bottom=min((y1+y2)*ScaleY+0.99f,iResY-1);

		iYAdjust=(y1+y2)-PSXDisplay.DisplayMode.y;
		if(iYAdjust>0)
			iYAdjust=(int)((float)iYAdjust*ScaleY)+1;
		else iYAdjust=0;

		gl_vy[0]=255-gl_vy[0];
		gl_vy[1]=255-gl_vy[1];
		gl_vy[2]=255-gl_vy[2];
		gl_vy[3]=255-gl_vy[3];

		y1=min(gl_vy[0],min(gl_vy[1],min(gl_vy[2],gl_vy[3])));

		gl_vy[0]-=y1;
		gl_vy[1]-=y1;
		gl_vy[2]-=y1;
		gl_vy[3]-=y1;
		gl_ux[0]-=gl_ux[7];
		gl_ux[1]-=gl_ux[7];
		gl_ux[2]-=gl_ux[7];
		gl_ux[3]-=gl_ux[7];

		/*
		ScaleX*=256.0f/((float)(iFTex));
		ScaleY*=256.0f/((float)(iFTex));
		*/
		
		ScaleX*=256.0f/((float)(iResX));
		ScaleY*=256.0f/((float)(iResY));

		y1=((float)gl_vy[0]*ScaleY); if(y1>255) y1=255;
		gl_vy[0]=y1;
		y1=((float)gl_vy[1]*ScaleY); if(y1>255) y1=255;
		gl_vy[1]=y1;
		y1=((float)gl_vy[2]*ScaleY); if(y1>255) y1=255;
		gl_vy[2]=y1;
		y1=((float)gl_vy[3]*ScaleY); if(y1>255) y1=255;
		gl_vy[3]=y1;

		x1=((float)gl_ux[0]*ScaleX); if(x1>255) x1=255;
		gl_ux[0]=x1;
		x1=((float)gl_ux[1]*ScaleX); if(x1>255) x1=255;
		gl_ux[1]=x1;
		x1=((float)gl_ux[2]*ScaleX); if(x1>255) x1=255;
		gl_ux[2]=x1;
		x1=((float)gl_ux[3]*ScaleX); if(x1>255) x1=255;
		gl_ux[3]=x1;

		x1=rSrc.right-rSrc.left;
		if(x1<=0)             x1=1;
		if(x1>iResX)          x1=iResX;

		y1=rSrc.bottom-rSrc.top;
		if(y1<=0)             y1=1;
		if(y1+iYAdjust>iResY) y1=iResY-iYAdjust;

#if 0
		if(bFakeFrontBuffer) glReadBuffer(GL_FRONT);

		glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 
			0,
			iYAdjust,
			rSrc.left+rRatioRect.left,
			iResY-rSrc.bottom-rRatioRect.top,
			x1,y1);

		if(glGetError()) 
		{
			char * p=(char *)malloc(iFTex*iFTex*4);
			memset(p,0,iFTex*iFTex*4);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, iFTex, iFTex,
				GL_RGB, GL_UNSIGNED_BYTE, p);
			free(p);
		}

		if(bFakeFrontBuffer) 
		{glReadBuffer(GL_BACK);bIgnoreNextTile=TRUE;}
#elif 0
		D3DPERF_BeginEvent(D3DCOLOR_ARGB(0xFF, 0xFF, 0, 0), L"BK_GetFrameBuffer");
		gpu_fb = BK_GetFrameBuffer();
		D3DPERF_EndEvent();
		// update uv coordonates (add offsets)


		if(bFakeFrontBuffer) {
			bIgnoreNextTile=TRUE;
		}
#endif
		ubOpaqueDraw=0;

		if(iSpriteTex)
		{
			sprtW=gl_ux[1]-gl_ux[0];    
			sprtH=-(gl_vy[0]-gl_vy[2]);
		}
		return NULL;
		return gTexName;
#else
	return NULL;
#endif
}


/////////////////////////////////////////////////////////////////////////////

int DefineSubTextureSort(bk_texture_t * tex)
{
	return BK_SetSubTextureData(tex, XTexS, YTexS,
		DXTexS, DYTexS, texturepart);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// load texture part (unpacked)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

int LoadSubTexturePageSort(int pageid, int mode, short cx, short cy, bk_texture_t * tex)
{
	uint32_t       start,row,column,j,sxh,sxm;
	unsigned int   palstart;
	uint32_t       *px,*pa,*ta;
	unsigned char  *cSRCPtr;
	unsigned short *wSRCPtr;
	uint32_t       LineOffset;
	uint32_t       x2a,xalign=0;
	uint32_t       x1=gl_ux[7];
	uint32_t       x2=gl_ux[6];
	uint32_t       y1=gl_ux[5];
	uint32_t       y2=gl_ux[4];
	uint32_t       dx=x2-x1+1;
	uint32_t       dy=y2-y1+1;
	int pmult=pageid/16;
	uint32_t      (*LTCOL)(uint32_t);
	unsigned int a,r,g,b,cnt,h;
	uint32_t      scol[8];

	LTCOL=TCF[DrawSemiTrans];

	pa=px=(uint32_t *)ubPaletteBuffer;
	ta=(uint32_t *)texturepart;
	palstart=cx+(cy<<10);

	ubOpaqueDraw=0;

	if(YTexS) {ta+=dx;if(XTexS) ta+=2;}
	if(XTexS) {ta+=1;xalign=2;}

	switch(mode)
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

			for(TXV=y1;TXV<=y2;TXV++)
			{
				for(TXU=x1;TXU<=x2;TXU++)
				{
					n_xi = ( ( TXU >> 2 ) & ~0x3c ) + ( ( TXV << 2 ) & 0x3c );
					n_yi = ( TXV & ~0xf ) + ( ( TXU >> 4 ) & 0xf );

					*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
				}
				ta+=xalign;
			}
			break;
		}

		start=((pageid-16*pmult)<<7)+524288*pmult;
		// convert CLUT to 32bits .. and then use THAT as a lookup table

		wSRCPtr=psxVuw+palstart;

		row=4;do
		{
			*px = (LTCOL(ptr32(wSRCPtr)));
            *(px + 1) = (LTCOL(ptr32(wSRCPtr + 1)));
            *(px + 2) = (LTCOL(ptr32(wSRCPtr + 2)));
            *(px + 3) = (LTCOL(ptr32(wSRCPtr + 3)));
			row--;px+=4;wSRCPtr+=4;
		}
		while (row);

		x2a=x2?(x2-1):0;//if(x2) x2a=x2-1; else x2a=0;
		sxm=x1&1;sxh=x1>>1;
		j=sxm?(x1+1):x1;//if(sxm) j=x1+1; else j=x1;
		for(column=y1;column<=y2;column++)
		{
			cSRCPtr = psxVub + start + (column<<11) + sxh;

			if(sxm) *ta++=*(pa+((*cSRCPtr++ >> 4) & 0xF));

			for(row=j;row<x2a;row+=2)
			{
				*ta    =*(pa+(*cSRCPtr & 0xF)); 
				*(ta+1)=*(pa+((*cSRCPtr >> 4) & 0xF)); 
				cSRCPtr++;ta+=2;
			}

			if(row<=x2) 
			{
				*ta++=*(pa+(*cSRCPtr & 0xF)); row++;
				if(row<=x2) *ta++=*(pa+((*cSRCPtr >> 4) & 0xF));
			}

			ta+=xalign;
		}

		break;
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

			for(TXV=y1;TXV<=y2;TXV++)
			{
				for(TXU=x1;TXU<=x2;TXU++)
				{
					n_xi = ( ( TXU >> 1 ) & ~0x78 ) + ( ( TXU << 2 ) & 0x40 ) + ( ( TXV << 3 ) & 0x38 );
					n_yi = ( TXV & ~0x7 ) + ( ( TXU >> 5 ) & 0x7 );

					*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff));
				}
				ta+=xalign;
			}

			break;
		}

		start=((pageid-16*pmult)<<7)+524288*pmult;

		cSRCPtr = psxVub + start + (y1<<11) + x1;
		LineOffset = 2048 - dx; 

		if(dy*dx>384)
		{
			wSRCPtr=psxVuw+palstart;

			row=64;do
			{
				*px = (LTCOL(ptr32(wSRCPtr)));
                *(px + 1) = (LTCOL(ptr32(wSRCPtr + 1)));
                *(px + 2) = (LTCOL(ptr32(wSRCPtr + 2)));
                *(px + 3) = (LTCOL(ptr32(wSRCPtr + 3)));
				row--;px+=4;wSRCPtr+=4;
			}
			while (row);

			column=dy;do 
			{
				row=dx;
				do {*ta++=*(pa+(*cSRCPtr++));row--;} while(row);
				ta+=xalign;
				cSRCPtr+=LineOffset;column--;
			}
			while(column);
		}
		else
		{
			wSRCPtr=psxVuw+palstart;

			column=dy;do 
			{
				row=dx;
                    do {
                        *ta++ = LTCOL(ptr32(wSRCPtr + *cSRCPtr++));
                        row--;
                    } while (row);
				ta+=xalign;
				cSRCPtr+=LineOffset;column--;
			}
			while(column);
		}

		break;
		//--------------------------------------------------// 
		// 16bit texture load ..
	case 2:
		start=((pageid-16*pmult)<<6)+262144*pmult;

		wSRCPtr = psxVuw + start + (y1<<10) + x1;
		LineOffset = 1024 - dx; 

		column=dy;do 
		{
			row=dx;
                do {
                    *ta++ = (LTCOL(ptr32(wSRCPtr++)));
                    row--;
                } while (row);
			ta+=xalign;
			wSRCPtr+=LineOffset;column--;
		}
		while(column);

		break;
		//--------------------------------------------------//
		// others are not possible !
	}

	x2a=dx+xalign;

	if(YTexS)
	{
		ta=(uint32_t *)texturepart;
		pa=(uint32_t *)texturepart+x2a;
		row=x2a;do {*ta++=*pa++;row--;} while(row);        
		pa=(uint32_t *)texturepart+dy*x2a;
		ta=pa+x2a;
		row=x2a;do {*ta++=*pa++;row--;} while(row);
		YTexS--;
		dy+=2;
	}

	if(XTexS)
	{
		ta=(uint32_t *)texturepart;
		pa=ta+1;
		row=dy;do {*ta=*pa;ta+=x2a;pa+=x2a;row--;} while(row);
		pa=(uint32_t *)texturepart+dx;
		ta=pa+1;
		row=dy;do {*ta=*pa;ta+=x2a;pa+=x2a;row--;} while(row);
		XTexS--;
		dx+=2;
	}

	DXTexS=dx;DYTexS=dy;

	if(!iFilterType) {		
		return DefineSubTextureSort(tex);
	}
	if(iFilterType!=2 && iFilterType!=4 && iFilterType!=6) {
		return DefineSubTextureSort(tex);
	}
	if((iFilterType==4 || iFilterType==6) && ly0==ly1 && ly2==ly3 && lx0==lx3 && lx1==lx2) {
		return DefineSubTextureSort(tex);
	}

	ta=(uint32_t *)texturepart;
	x1=dx-1;
	y1=dy-1;

	if(bOpaquePass)
	{
		if(0)
		{
			for(column=0;column<dy;column++)
			{
				for(row=0;row<dx;row++)
				{
					if(*ta==0x03000000)
					{
						cnt=0;

						if(           column     && *(ta-dx)  >>24 !=0x03) scol[cnt++]=*(ta-dx);
						if(row                   && *(ta-1)   >>24 !=0x03) scol[cnt++]=*(ta-1);
						if(row!=x1               && *(ta+1)   >>24 !=0x03) scol[cnt++]=*(ta+1);
						if(           column!=y1 && *(ta+dx)  >>24 !=0x03) scol[cnt++]=*(ta+dx);

						if(row     && column     && *(ta-dx-1)>>24 !=0x03) scol[cnt++]=*(ta-dx-1);
						if(row!=x1 && column     && *(ta-dx+1)>>24 !=0x03) scol[cnt++]=*(ta-dx+1);
						if(row     && column!=y1 && *(ta+dx-1)>>24 !=0x03) scol[cnt++]=*(ta+dx-1);
						if(row!=x1 && column!=y1 && *(ta+dx+1)>>24 !=0x03) scol[cnt++]=*(ta+dx+1);

						if(cnt)
						{
							r=g=b=a=0;
							for(h=0;h<cnt;h++)
							{
								r+=(scol[h]>>16)&0xff;
								g+=(scol[h]>>8)&0xff;
								b+=scol[h]&0xff;
							}
							r/=cnt;b/=cnt;g/=cnt;

							*ta=(r<<16)|(g<<8)|b;
							*ta|=0x03000000;
						}
					}
					ta++;
				}
			}
		}
		else
		{
			for(column=0;column<dy;column++)
			{
				for(row=0;row<dx;row++)
				{
					if(*ta==0x50000000)
					{
						cnt=0;

						if(           column     && *(ta-dx)  !=0x50000000 && *(ta-dx)>>24!=1) scol[cnt++]=*(ta-dx);
						if(row                   && *(ta-1)   !=0x50000000 && *(ta-1)>>24!=1) scol[cnt++]=*(ta-1);
						if(row!=x1               && *(ta+1)   !=0x50000000 && *(ta+1)>>24!=1) scol[cnt++]=*(ta+1);
						if(           column!=y1 && *(ta+dx)  !=0x50000000 && *(ta+dx)>>24!=1) scol[cnt++]=*(ta+dx);

						if(row     && column     && *(ta-dx-1)!=0x50000000 && *(ta-dx-1)>>24!=1) scol[cnt++]=*(ta-dx-1);
						if(row!=x1 && column     && *(ta-dx+1)!=0x50000000 && *(ta-dx+1)>>24!=1) scol[cnt++]=*(ta-dx+1);
						if(row     && column!=y1 && *(ta+dx-1)!=0x50000000 && *(ta+dx-1)>>24!=1) scol[cnt++]=*(ta+dx-1);
						if(row!=x1 && column!=y1 && *(ta+dx+1)!=0x50000000 && *(ta+dx+1)>>24!=1) scol[cnt++]=*(ta+dx+1);

						if(cnt)
						{
							r=g=b=a=0;
							for(h=0;h<cnt;h++)
							{
								a+=(scol[h]>>24);
								r+=(scol[h]>>16)&0xff;
								g+=(scol[h]>>8)&0xff;
								b+=scol[h]&0xff;
							}
							r/=cnt;b/=cnt;g/=cnt;

							*ta=(r<<16)|(g<<8)|b;
							if(a) *ta|=0x50000000;
							else  *ta|=0x01000000;
						}
					}
					ta++;
				}
			}
		}
	}
	else
		for(column=0;column<dy;column++)
		{
			for(row=0;row<dx;row++)
			{
				if(*ta==0x00000000)
				{
					cnt=0;

					if(row!=x1               && *(ta+1)   !=0x00000000) scol[cnt++]=*(ta+1);
					if(           column!=y1 && *(ta+dx)  !=0x00000000) scol[cnt++]=*(ta+dx);

					if(cnt)
					{
						r=g=b=0;
						for(h=0;h<cnt;h++)
						{
							r+=(scol[h]>>16)&0xff;
							g+=(scol[h]>>8)&0xff;
							b+=scol[h]&0xff;
						}
						r/=cnt;b/=cnt;g/=cnt;
						*ta=(r<<16)|(g<<8)|b;
					}
				}
				ta++;
			}
		}

		return DefineSubTextureSort(tex);
}

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// texture cache garbage collection
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void DoTexGarbageCollection(void)
{
	static unsigned short LRUCleaned=0;
	unsigned short iC,iC1,iC2;
	int i,j,iMax;textureSubCacheEntryS * tsb;

	iC=4;//=iSortTexCnt/2,
	LRUCleaned+=iC;                                       // we clean different textures each time
	if((LRUCleaned+iC)>=iSortTexCnt) LRUCleaned=0;        // wrap? wrap!
	iC1=LRUCleaned;                                       // range of textures to clean
	iC2=LRUCleaned+iC;

	for(iC=iC1;iC<iC2;iC++)                               // make some textures available
	{
		pxSsubtexLeft[iC]->l=0;
	}

	for(i=0;i<3;i++)                                      // remove all references to that textures
		for(j=0;j<MAXTPAGES;j++)
			for(iC=0;iC<4;iC++)                                 // loop all texture rect info areas
			{
				tsb=pscSubtexStore[i][j]+(iC*SOFFB);
				iMax=tsb->pos.l;
				if(iMax)
					do
					{
						tsb++;
						if(tsb->cTexID>=iC1 && tsb->cTexID<iC2)        // info uses the cleaned textures? remove info
							tsb->ClutID=0;
					} 
					while(--iMax);
			}

			usLRUTexPage=LRUCleaned;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// search cache for existing (already used) parts
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

unsigned char * CheckTextureInSubSCache(int TextureMode, uint32_t GivenClutId, unsigned short * pCache)
{
	textureSubCacheEntryS * tsx, * tsb, *tsg;//, *tse=NULL;
	int i,iMax;EXLong npos;
	unsigned char cx,cy;
	int iC,j,k;uint32_t rx,ry,mx,my;
	EXLong * ul=0, * uls;
	EXLong rfree;
	unsigned char cXAdj,cYAdj;

    npos.l = GETLE32((uint32_t *) & gl_ux[4]);

	//--------------------------------------------------------------//
	// find matching texturepart first... speed up...
	//--------------------------------------------------------------//

	tsg=pscSubtexStore[TextureMode][GlobalTexturePage];
	tsg+=((GivenClutId&CLUTCHK)>>CLUTSHIFT)*SOFFB;

	iMax=tsg->pos.l;
	if(iMax)
	{
		i=iMax;
		tsb=tsg+1;                 
		do
		{
			if(GivenClutId==tsb->ClutID &&
				(INCHECK(tsb->pos,npos)))
			{
				{
					cx=tsb->pos._3-tsb->posTX;
					cy=tsb->pos._1-tsb->posTY;

					gl_ux[0]-=cx;
					gl_ux[1]-=cx;
					gl_ux[2]-=cx;
					gl_ux[3]-=cx;
					gl_vy[0]-=cy;
					gl_vy[1]-=cy;
					gl_vy[2]-=cy;
					gl_vy[3]-=cy;

					ubOpaqueDraw=tsb->Opaque;
					*pCache=tsb->cTexID;
					return NULL;
				}
			} 
			tsb++;
		}
		while(--i);
	}

	//----------------------------------------------------//

	cXAdj=1;cYAdj=1;

	rx=(int)gl_ux[6]-(int)gl_ux[7];
	ry=(int)gl_ux[4]-(int)gl_ux[5];

	tsx=NULL;tsb=tsg+1;
	for(i=0;i<iMax;i++,tsb++)
	{
		if(!tsb->ClutID) {tsx=tsb;break;}
	}

	if(!tsx) 
	{
		iMax++;
		if(iMax>=SOFFB-2) 
		{
			if(iTexGarbageCollection)                         // gc mode?
			{
				if(*pCache==0) 
				{
					dwTexPageComp|=(1<<GlobalTexturePage);
					*pCache=0xffff;
					return 0;
				}

				iMax--;
				tsb=tsg+1;

				for(i=0;i<iMax;i++,tsb++)                       // 1. search other slots with same cluts, and unite the area
					if(GivenClutId==tsb->ClutID)
					{
						if(!tsx) {tsx=tsb;rfree.l=npos.l;}           // 
						else      tsb->ClutID=0;
						rfree._3=min(rfree._3,tsb->pos._3);
						rfree._2=max(rfree._2,tsb->pos._2);
						rfree._1=min(rfree._1,tsb->pos._1);
						rfree._0=max(rfree._0,tsb->pos._0);
						MarkFree(tsb);
					}

					if(tsx)                                         // 3. if one or more found, create a new rect with bigger size
					{
						npos.l = rfree.l;
                    	PUTLE32(((uint32_t *) & gl_ux[4]), rfree.l);
						rx=(int)rfree._2-(int)rfree._3;
						ry=(int)rfree._0-(int)rfree._1;
						DoTexGarbageCollection();

						goto ENDLOOP3;
					}
			}

			iMax=1;
		}
		tsx=tsg+iMax;
		tsg->pos.l=iMax;
	}

	//----------------------------------------------------//
	// now get a free texture space
	//----------------------------------------------------//

	if(iTexGarbageCollection) usLRUTexPage=0;

ENDLOOP3:

	rx+=3;if(rx>255) {cXAdj=0;rx=255;}
	ry+=3;if(ry>255) {cYAdj=0;ry=255;}

	iC=usLRUTexPage;

	for(k=0;k<iSortTexCnt;k++)
	{
		uls=pxSsubtexLeft[iC];
		iMax=uls->l;ul=uls+1;

		//--------------------------------------------------//
		// first time

		if(!iMax) 
		{
			rfree.l=0;

			if(rx>252 && ry>252)
			{uls->l=1;ul->l=0xffffffff;ul=0;goto ENDLOOP;}

			if(rx<253)
			{
				uls->l=uls->l+1;
				ul->_3=rx;
				ul->_2=255-rx;
				ul->_1=0;
				ul->_0=ry;
				ul++;
			}

			if(ry<253)
			{
				uls->l=uls->l+1; 
				ul->_3=0;
				ul->_2=255;
				ul->_1=ry;
				ul->_0=255-ry;
			}
			ul=0;
			goto ENDLOOP;
		}

		//--------------------------------------------------//
		for(i=0;i<iMax;i++,ul++)
		{
			if(ul->l!=0xffffffff && 
				ry<=ul->_0      && 
				rx<=ul->_2)
			{
				rfree=*ul;
				mx=ul->_2-2;
				my=ul->_0-2;
				if(rx<mx && ry<my)
				{
					ul->_3+=rx;
					ul->_2-=rx;
					ul->_0=ry;

					for(ul=uls+1,j=0;j<iMax;j++,ul++)
						if(ul->l==0xffffffff) break;

					if(j<CSUBSIZE-2)
					{
						if(j==iMax) uls->l=uls->l+1;

						ul->_3=rfree._3;
						ul->_2=rfree._2;
						ul->_1=rfree._1+ry;
						ul->_0=rfree._0-ry;
					}
				}
				else if(rx<mx)
				{
					ul->_3+=rx;
					ul->_2-=rx;
				}
				else if(ry<my)
				{
					ul->_1+=ry;
					ul->_0-=ry;
				}
				else
				{
					ul->l=0xffffffff;
				}
				ul=0;
				goto ENDLOOP;
			}
		}

		//--------------------------------------------------//

		iC++; if(iC>=iSortTexCnt) iC=0;
	}

	//----------------------------------------------------//
	// check, if free space got
	//----------------------------------------------------//

ENDLOOP:
	if(ul)
	{
		//////////////////////////////////////////////////////

		{
			dwTexPageComp=0;

			for(i=0;i<3;i++)                                    // cleaning up
				for(j=0;j<MAXTPAGES;j++)
				{
					tsb=pscSubtexStore[i][j];
					(tsb+SOFFA)->pos.l=0;
					(tsb+SOFFB)->pos.l=0;
					(tsb+SOFFC)->pos.l=0;
					(tsb+SOFFD)->pos.l=0;
				}
				for(i=0;i<iSortTexCnt;i++)
				{ul=pxSsubtexLeft[i];ul->l=0;}
				usLRUTexPage=0;
		}

		//////////////////////////////////////////////////////
		iC=usLRUTexPage;
		uls=pxSsubtexLeft[usLRUTexPage];
		uls->l=0;ul=uls+1;
		rfree.l=0;

		if(rx>252 && ry>252)
		{uls->l=1;ul->l=0xffffffff;}
		else
		{
			if(rx<253)
			{
				uls->l=uls->l+1;
				ul->_3=rx;
				ul->_2=255-rx;
				ul->_1=0;
				ul->_0=ry;
				ul++;
			}
			if(ry<253)
			{
				uls->l=uls->l+1; 
				ul->_3=0;
				ul->_2=255;
				ul->_1=ry;
				ul->_0=255-ry;
			}
		}
		tsg->pos.l=1;tsx=tsg+1;
	}

	rfree._3+=cXAdj;
	rfree._1+=cYAdj;

	tsx->cTexID   =*pCache=iC;
	tsx->pos      = npos;
	tsx->ClutID   = GivenClutId;
	tsx->posTX    = rfree._3;
	tsx->posTY    = rfree._1;

	cx=gl_ux[7]-rfree._3;
	cy=gl_ux[5]-rfree._1;

	gl_ux[0]-=cx;
	gl_ux[1]-=cx;
	gl_ux[2]-=cx;
	gl_ux[3]-=cx;
	gl_vy[0]-=cy;
	gl_vy[1]-=cy;
	gl_vy[2]-=cy;
	gl_vy[3]-=cy;

	XTexS=rfree._3;
	YTexS=rfree._1;

	return &tsx->Opaque;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// search cache for free place (on compress)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

BOOL GetCompressTexturePlace(textureSubCacheEntryS * tsx)
{
	int i,j,k,iMax,iC;uint32_t rx,ry,mx,my;
	EXLong * ul=0, * uls, rfree;
	unsigned char cXAdj=1,cYAdj=1;

	rx=(int)tsx->pos._2-(int)tsx->pos._3;
	ry=(int)tsx->pos._0-(int)tsx->pos._1;

	rx+=3;if(rx>255) {cXAdj=0;rx=255;}
	ry+=3;if(ry>255) {cYAdj=0;ry=255;}

	iC=usLRUTexPage;

	for(k=0;k<iSortTexCnt;k++)
	{
		uls=pxSsubtexLeft[iC];
		iMax=uls->l;ul=uls+1;

		//--------------------------------------------------//
		// first time

		if(!iMax)
		{
			rfree.l=0;

			if(rx>252 && ry>252)
			{uls->l=1;ul->l=0xffffffff;ul=0;goto TENDLOOP;}

			if(rx<253)
			{
				uls->l=uls->l+1;
				ul->_3=rx;
				ul->_2=255-rx;
				ul->_1=0;
				ul->_0=ry;
				ul++;
			}

			if(ry<253)
			{
				uls->l=uls->l+1;
				ul->_3=0;
				ul->_2=255;
				ul->_1=ry;
				ul->_0=255-ry;
			}
			ul=0;
			goto TENDLOOP;
		}

		//--------------------------------------------------//
		for(i=0;i<iMax;i++,ul++)
		{
			if(ul->l!=0xffffffff &&
				ry<=ul->_0      &&
				rx<=ul->_2)
			{
				rfree=*ul;
				mx=ul->_2-2;
				my=ul->_0-2;

				if(rx<mx && ry<my)
				{
					ul->_3+=rx;
					ul->_2-=rx;
					ul->_0=ry;

					for(ul=uls+1,j=0;j<iMax;j++,ul++)
						if(ul->l==0xffffffff) break;

					if(j<CSUBSIZE-2)
					{
						if(j==iMax) uls->l=uls->l+1;

						ul->_3=rfree._3;
						ul->_2=rfree._2;
						ul->_1=rfree._1+ry;
						ul->_0=rfree._0-ry;
					}
				}
				else if(rx<mx)
				{
					ul->_3+=rx;
					ul->_2-=rx;
				}
				else if(ry<my)
				{
					ul->_1+=ry;
					ul->_0-=ry;
				}
				else
				{
					ul->l=0xffffffff;
				}
				ul=0;
				goto TENDLOOP;
			}
		}

		//--------------------------------------------------//

		iC++; if(iC>=iSortTexCnt) iC=0;
	}

	//----------------------------------------------------//
	// check, if free space got
	//----------------------------------------------------//

TENDLOOP:
	if(ul) return FALSE;

	rfree._3+=cXAdj;
	rfree._1+=cYAdj;

	tsx->cTexID   = iC;
	tsx->posTX    = rfree._3;
	tsx->posTY    = rfree._1;

	XTexS=rfree._3;
	YTexS=rfree._1;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// compress texture cache (to make place for new texture part, if needed)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CompressTextureSpace(void)
{
	bk_texture_t * tex = NULL;
	textureSubCacheEntryS * tsx, * tsg, * tsb;
	int i,j,k,m,n,iMax;EXLong * ul, r,opos;
	short sOldDST=DrawSemiTrans,cx,cy;
	int  lOGTP=GlobalTexturePage;
	uint32_t l,row;
	uint32_t *lSRCPtr;

    opos.l = GETLE32((uint32_t *) & gl_ux[4]);

	// 1. mark all textures as free
	for(i=0;i<iSortTexCnt;i++)
	{ul=pxSsubtexLeft[i];ul->l=0;}
	usLRUTexPage=0;

	// 2. compress
	for(j=0;j<3;j++)
	{
		for(k=0;k<MAXTPAGES;k++)
		{
			tsg=pscSubtexStore[j][k];

			if((!(dwTexPageComp&(1<<k))))
			{
				(tsg+SOFFA)->pos.l=0;
				(tsg+SOFFB)->pos.l=0;
				(tsg+SOFFC)->pos.l=0;
				(tsg+SOFFD)->pos.l=0;
				continue;
			}

			for(m=0;m<4;m++,tsg+=SOFFB)
			{
				iMax=tsg->pos.l;

				tsx=tsg+1;
				for(i=0;i<iMax;i++,tsx++)
				{
					if(tsx->ClutID)
					{
						r.l=tsx->pos.l;
						for(n=i+1,tsb=tsx+1;n<iMax;n++,tsb++)
						{
							if(tsx->ClutID==tsb->ClutID)
							{
								r._3=min(r._3,tsb->pos._3);
								r._2=max(r._2,tsb->pos._2);
								r._1=min(r._1,tsb->pos._1);
								r._0=max(r._0,tsb->pos._0);
								tsb->ClutID=0;
							}
						}

						//           if(r.l!=tsx->pos.l)
						{
							cx=((tsx->ClutID << 4) & 0x3F0);          
							cy=((tsx->ClutID >> 6) & CLUTYMASK);

							if(j!=2)
							{
								// palette check sum
								l=0;lSRCPtr=(uint32_t *)(psxVuw+cx+(cy*1024));
								if(j==1) for(row=1;row<129;row++) l+=((*lSRCPtr++)-1)*row;
								else     for(row=1;row<9;row++)   l+=((*lSRCPtr++)-1)<<row;
								l=((l+HIWORD(l))&0x3fffL)<<16;
								if(l!=(tsx->ClutID&(0x00003fff<<16)))
								{
									tsx->ClutID=0;continue;
								}
							}

							tsx->pos.l=r.l;
							if(!GetCompressTexturePlace(tsx))         // no place?
							{
								for(i=0;i<3;i++)                        // -> clean up everything
									for(j=0;j<MAXTPAGES;j++)
									{
										tsb=pscSubtexStore[i][j];
										(tsb+SOFFA)->pos.l=0;
										(tsb+SOFFB)->pos.l=0;
										(tsb+SOFFC)->pos.l=0;
										(tsb+SOFFD)->pos.l=0;
									}
									for(i=0;i<iSortTexCnt;i++)
									{ul=pxSsubtexLeft[i];ul->l=0;}
									usLRUTexPage=0;
									DrawSemiTrans=sOldDST;
									GlobalTexturePage=lOGTP;

                                	PUTLE32(((uint32_t *) & gl_ux[4]), opos.l);

									dwTexPageComp=0;

									return;
							}

							if(tsx->ClutID&(1<<30)) DrawSemiTrans=1;
							else                    DrawSemiTrans=0;
							PUTLE32(((uint32_t *) & gl_ux[4]), r.l);

                            gTexName = uiStexturePage[tsx->cTexID];
							LoadSubTexFn(k,j,cx,cy);
							uiStexturePage[tsx->cTexID]=tex;
							tsx->Opaque=ubOpaqueDraw;
						}
					}
				}

				if(iMax)  
				{
					tsx=tsg+iMax;
					while(!tsx->ClutID && iMax) {tsx--;iMax--;}
					tsg->pos.l=iMax;
				}

			}                      
		}
	}

	if(dwTexPageComp==0xffffffff) dwTexPageComp=0;

    PUTLE32(((uint32_t *) & gl_ux[4]), opos.l);

	GlobalTexturePage=lOGTP;
	DrawSemiTrans=sOldDST;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// main entry for searching/creating textures, called from prim.c
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

bk_texture_t * SelectSubTextureS(int TextureMode, uint32_t GivenClutId) 
{
	bk_texture_t * tex = NULL;
	unsigned char * OPtr;unsigned short iCache;short cx,cy;

	// sort sow/tow infos for fast access

	unsigned char ma1,ma2,mi1,mi2;
	if(gl_ux[0]>gl_ux[1]) {mi1=gl_ux[1];ma1=gl_ux[0];}
	else                  {mi1=gl_ux[0];ma1=gl_ux[1];}
	if(gl_ux[2]>gl_ux[3]) {mi2=gl_ux[3];ma2=gl_ux[2];}
	else                  {mi2=gl_ux[2];ma2=gl_ux[3];}
	if(mi1>mi2) gl_ux[7]=mi2; 
	else        gl_ux[7]=mi1;
	if(ma1>ma2) gl_ux[6]=ma1; 
	else        gl_ux[6]=ma2;

	if(gl_vy[0]>gl_vy[1]) {mi1=gl_vy[1];ma1=gl_vy[0];}
	else                  {mi1=gl_vy[0];ma1=gl_vy[1];}
	if(gl_vy[2]>gl_vy[3]) {mi2=gl_vy[3];ma2=gl_vy[2];}
	else                  {mi2=gl_vy[2];ma2=gl_vy[3];}
	if(mi1>mi2) gl_ux[5]=mi2; 
	else        gl_ux[5]=mi1;
	if(ma1>ma2) gl_ux[4]=ma1; 
	else        gl_ux[4]=ma2;

	// get clut infos in one 32 bit val

	if(TextureMode==2)                                    // no clut here
	{
		GivenClutId=CLUTUSED|(DrawSemiTrans<<30);cx=cy=0;
		/*
		if(iFrameTexType) {
			return Fake15BitTexture();
		} else {
			return FakeFramebuffer();
		}
		*/
	}           
	else 
	{
		cx=((GivenClutId << 4) & 0x3F0);                    // but here
		cy=((GivenClutId >> 6) & CLUTYMASK);
		GivenClutId=(GivenClutId&CLUTMASK)|(DrawSemiTrans<<30)|CLUTUSED;

		 // palette check sum.. removed MMX asm, this easy func works as well
        {
            uint32_t l = 0, row;

            uint32_t *lSRCPtr = (uint32_t *) (psxVuw + cx + (cy * 1024));
            if (TextureMode == 1) for (row = 1; row < 129; row++) l += ((GETLE32(lSRCPtr++)) - 1) * row;
            else for (row = 1; row < 9; row++) l += ((GETLE32(lSRCPtr++)) - 1) << row;
            l = (l + HIWORD(l))&0x3fffL;
            GivenClutId |= (l << 16);
        }

	}

	// search cache
	iCache=0;
	OPtr=CheckTextureInSubSCache(TextureMode,GivenClutId,&iCache);

	// cache full? compress and try again
	if(iCache==0xffff)
	{
		CompressTextureSpace();
		OPtr=CheckTextureInSubSCache(TextureMode,GivenClutId,&iCache);
	}

	// found? fine
	usLRUTexPage=iCache;

	if(!OPtr) {
		return uiStexturePage[iCache];
	}

	// not found? upload texture and store infos in cache
	tex=uiStexturePage[iCache];
	//LoadSubTexFn(GlobalTexturePage,TextureMode,cx,cy);
	if (tex == NULL) {
		tex = BK_CreateTexture(256, 256);
	}
	LoadSubTexturePageSort(GlobalTexturePage, TextureMode, cx, cy, tex);
	uiStexturePage[iCache]=tex;
	*OPtr=ubOpaqueDraw;

	return tex;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

