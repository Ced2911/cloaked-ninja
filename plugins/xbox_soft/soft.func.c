/**
* todo: cleanup
**/
#include <xtl.h>
#include "externals.h"
#include "soft.h"

//#define VC_INLINE
#include "gpu.h"
#include "prim.h"
#include "menu.h"
#include "swap.h"

#include "soft.func.h"

#define USE_NEW_FUNC

__inline void __GetTextureTransColG32CheckMask(uint32_t * pdest,uint32_t color, uint32_t l, uint32_t r,uint32_t g, uint32_t b) {
	uint32_t ma=GETLE32(pdest);

	PUTLE32(pdest, (X32PSXCOL(r,g,b))|l);

	if((color&0xffff)==0    ) PUTLE32(pdest, (ma&0xffff)|(GETLE32(pdest)&0xffff0000));
	if((color&0xffff0000)==0) PUTLE32(pdest, (ma&0xffff0000)|(GETLE32(pdest)&0xffff));
	if(ma&0x80000000) PUTLE32(pdest, (ma&0xFFFF0000)|(GETLE32(pdest)&0xFFFF));
	if(ma&0x00008000) PUTLE32(pdest, (ma&0xFFFF)    |(GETLE32(pdest)&0xFFFF0000));
}

__inline void __GetTextureTransColG320x8000(uint32_t color, uint32_t *r,uint32_t *g, uint32_t *b) {
	*r=(*r&0xffff0000)|((((X32COL1(color))* g_m1)&0x0000FF80)>>7);
	*b=(*b&0xffff0000)|((((X32COL2(color))* g_m2)&0x0000FF80)>>7);
	*g=(*g&0xffff0000)|((((X32COL3(color))* g_m3)&0x0000FF80)>>7);
}

__inline void __GetTextureTransColG320x80000000(uint32_t color, uint32_t *r,uint32_t *g, uint32_t *b) {
	*r=(*r&0xffff)|((((X32COL1(color))* g_m1)&0xFF800000)>>7);
	*b=(*b&0xffff)|((((X32COL2(color))* g_m2)&0xFF800000)>>7);
	*g=(*g&0xffff)|((((X32COL3(color))* g_m3)&0xFF800000)>>7);
}

__inline void __GetTextureTransColG32Simple(uint32_t color, uint32_t *r,uint32_t *g, uint32_t *b) {
	*r=(((X32COL1(color))* g_m1)&0xFF80FF80)>>7;
	*b=(((X32COL2(color))* g_m2)&0xFF80FF80)>>7;
	*g=(((X32COL3(color))* g_m3)&0xFF80FF80)>>7;
}

__inline void GetTextureTransColG32ABR0_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	uint32_t r,g,b,l;

	if(color==0) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
		r=((((X32TCOL1(GETLE32(pdest)))+((X32COL1(color)) * g_m1))&0xFF00FF00)>>8);
		b=((((X32TCOL2(GETLE32(pdest)))+((X32COL2(color)) * g_m2))&0xFF00FF00)>>8);
		g=((((X32TCOL3(GETLE32(pdest)))+((X32COL3(color)) * g_m3))&0xFF00FF00)>>8);

		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}

	// Clamp
	__ClampRGB(r, g, b);
	
	PUTLE32(pdest, (X32PSXCOL(r,g,b))|l);
}

__inline void GetTextureTransColG32ABR0_CheckMask(uint32_t * pdest, uint32_t color) {
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
		r=((((X32TCOL1(GETLE32(pdest)))+((X32COL1(color)) * g_m1))&0xFF00FF00)>>8);
		b=((((X32TCOL2(GETLE32(pdest)))+((X32COL2(color)) * g_m2))&0xFF00FF00)>>8);
		g=((((X32TCOL3(GETLE32(pdest)))+((X32COL3(color)) * g_m3))&0xFF00FF00)>>8);

		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}
	
	// Clamp
	__ClampRGB(r, g, b);

	// Apply mask
	__GetTextureTransColG32CheckMask(pdest, color, l, r, g, b);
}
__inline void GetTextureTransColG32ABR1_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
		r=(X32COL1(GETLE32(pdest)))+(((((X32COL1(color)))* g_m1)&0xFF80FF80)>>7);
		b=(X32COL2(GETLE32(pdest)))+(((((X32COL2(color)))* g_m2)&0xFF80FF80)>>7);
		g=(X32COL3(GETLE32(pdest)))+(((((X32COL3(color)))* g_m3)&0xFF80FF80)>>7);

		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}
	
	// Clamp
	__ClampRGB(r, g, b);
	
	// No mask
	PUTLE32(pdest, (X32PSXCOL(r,g,b))|l);
}

__inline void GetTextureTransColG32ABR1_CheckMask(uint32_t * pdest, uint32_t color) {
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
		r=(X32COL1(GETLE32(pdest)))+(((((X32COL1(color)))* g_m1)&0xFF80FF80)>>7);
		b=(X32COL2(GETLE32(pdest)))+(((((X32COL2(color)))* g_m2)&0xFF80FF80)>>7);
		g=(X32COL3(GETLE32(pdest)))+(((((X32COL3(color)))* g_m3)&0xFF80FF80)>>7);

		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}
	
	// Clamp
	__ClampRGB(r, g, b);

	// Apply mask
	__GetTextureTransColG32CheckMask(pdest, color, l, r, g, b);
}

__inline void GetTextureTransColG32ABR2_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
		uint32_t t;
		r=(((((X32COL1(color)))* g_m1)&0xFF80FF80)>>7);
		t=(GETLE32(pdest)&0x001f0000)-(r&0x003f0000); if(t&0x80000000) t=0;
		r=(GETLE32(pdest)&0x0000001f)-(r&0x0000003f); if(r&0x80000000) r=0;
		r|=t;

		b=(((((X32COL2(color)))* g_m2)&0xFF80FF80)>>7);
		t=((GETLE32(pdest)>>5)&0x001f0000)-(b&0x003f0000); if(t&0x80000000) t=0;
		b=((GETLE32(pdest)>>5)&0x0000001f)-(b&0x0000003f); if(b&0x80000000) b=0;
		b|=t;

		g=(((((X32COL3(color)))* g_m3)&0xFF80FF80)>>7);
		t=((GETLE32(pdest)>>10)&0x001f0000)-(g&0x003f0000); if(t&0x80000000) t=0;
		g=((GETLE32(pdest)>>10)&0x0000001f)-(g&0x0000003f); if(g&0x80000000) g=0;
		g|=t;
		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}
	
	// Clamp
	__ClampRGB(r, g, b);
	
	// No mask
	PUTLE32(pdest, (X32PSXCOL(r,g,b))|l);
}

__inline void GetTextureTransColG32ABR2_CheckMask(uint32_t * pdest, uint32_t color) {
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
		uint32_t t;
		r=(((((X32COL1(color)))* g_m1)&0xFF80FF80)>>7);
		t=(GETLE32(pdest)&0x001f0000)-(r&0x003f0000); if(t&0x80000000) t=0;
		r=(GETLE32(pdest)&0x0000001f)-(r&0x0000003f); if(r&0x80000000) r=0;
		r|=t;

		b=(((((X32COL2(color)))* g_m2)&0xFF80FF80)>>7);
		t=((GETLE32(pdest)>>5)&0x001f0000)-(b&0x003f0000); if(t&0x80000000) t=0;
		b=((GETLE32(pdest)>>5)&0x0000001f)-(b&0x0000003f); if(b&0x80000000) b=0;
		b|=t;

		g=(((((X32COL3(color)))* g_m3)&0xFF80FF80)>>7);
		t=((GETLE32(pdest)>>10)&0x001f0000)-(g&0x003f0000); if(t&0x80000000) t=0;
		g=((GETLE32(pdest)>>10)&0x0000001f)-(g&0x0000003f); if(g&0x80000000) g=0;
		g|=t;

		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}
	
	// Clamp
	__ClampRGB(r, g, b);

	// Apply mask
	__GetTextureTransColG32CheckMask(pdest, color, l, r, g, b);
}


__inline void GetTextureTransColG32SemiTrans_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
#ifdef HALFBRIGHTMODE3
		r=(X32COL1(GETLE32(pdest)))+(((((X32BCOL1(color))>>2)* g_m1)&0xFF80FF80)>>7);
		b=(X32COL2(GETLE32(pdest)))+(((((X32BCOL2(color))>>2)* g_m2)&0xFF80FF80)>>7);
		g=(X32COL3(GETLE32(pdest)))+(((((X32BCOL3(color))>>2)* g_m3)&0xFF80FF80)>>7);
#else
		r=(X32COL1(GETLE32(pdest)))+(((((X32ACOL1(color))>>1)* g_m1)&0xFF80FF80)>>7);
		b=(X32COL2(GETLE32(pdest)))+(((((X32ACOL2(color))>>1)* g_m2)&0xFF80FF80)>>7);
		g=(X32COL3(GETLE32(pdest)))+(((((X32ACOL3(color))>>1)* g_m3)&0xFF80FF80)>>7);
#endif

		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}
	
	// Clamp
	__ClampRGB(r, g, b);
	
	// No mask
	PUTLE32(pdest, (X32PSXCOL(r,g,b))|l);
}

__inline void GetTextureTransColG32SemiTrans_CheckMask(uint32_t * pdest, uint32_t color) {
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	if(color&0x80008000)
	{
#ifdef HALFBRIGHTMODE3
		r=(X32COL1(GETLE32(pdest)))+(((((X32BCOL1(color))>>2)* g_m1)&0xFF80FF80)>>7);
		b=(X32COL2(GETLE32(pdest)))+(((((X32BCOL2(color))>>2)* g_m2)&0xFF80FF80)>>7);
		g=(X32COL3(GETLE32(pdest)))+(((((X32BCOL3(color))>>2)* g_m3)&0xFF80FF80)>>7);
#else
		r=(X32COL1(GETLE32(pdest)))+(((((X32ACOL1(color))>>1)* g_m1)&0xFF80FF80)>>7);
		b=(X32COL2(GETLE32(pdest)))+(((((X32ACOL2(color))>>1)* g_m2)&0xFF80FF80)>>7);
		g=(X32COL3(GETLE32(pdest)))+(((((X32ACOL3(color))>>1)* g_m3)&0xFF80FF80)>>7);
#endif

		if(!(color&0x8000))
		{
			__GetTextureTransColG320x8000(color, &r, &g, &b);
		}
		if(!(color&0x80000000))
		{
			__GetTextureTransColG320x80000000(color, &r, &g, &b);
		}

	}
	else 
	{
		__GetTextureTransColG32Simple(color, &r, &g, &b);
	}
	
	// Clamp
	__ClampRGB(r, g, b);

	// Apply mask
	__GetTextureTransColG32CheckMask(pdest, color, l, r, g, b);
}


__inline void GetTextureTransColG32NoSemiTrans_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	__GetTextureTransColG32Simple(color, &r, &g, &b);
	
	// Clamp
	__ClampRGB(r, g, b);
	
	// No mask
	PUTLE32(pdest, (X32PSXCOL(r,g,b))|l);
}

__inline void GetTextureTransColG32NoSemiTrans_CheckMask(uint32_t * pdest, uint32_t color) {
	uint32_t r,g,b,l;

	//if(color==0) return;
	if(!color) return;

	l=lSetMask|(color&0x80008000);

	__GetTextureTransColG32Simple(color, &r, &g, &b);
	
	// Clamp
	__ClampRGB(r, g, b);

	// Apply mask
	__GetTextureTransColG32CheckMask(pdest, color, l, r, g, b);
}


t_getshadetrans_func GetTextureTransColG32Func() {
	// By default take the slow one ...
	t_getshadetrans_func func = GetTextureTransColG32;

#ifdef USE_NEW_FUNC
	if(DrawSemiTrans) {
		if (GlobalTextABR==0)
		{
			if (bCheckMask) {
				func = GetTextureTransColG32ABR0_CheckMask;
			} else {
				func = GetTextureTransColG32ABR0_NoCheckMask; 
			}
			
		} else if(GlobalTextABR==1) {
			if (bCheckMask) {
				func = GetTextureTransColG32ABR1_CheckMask;
			} else {
				func = GetTextureTransColG32ABR1_NoCheckMask; 
			}
		} else if(GlobalTextABR==2) {
			if (bCheckMask) {
				func = GetTextureTransColG32ABR2_CheckMask;
			} else {
				func = GetTextureTransColG32ABR2_NoCheckMask; 
			}
		} else {
			if (bCheckMask) {
				func = GetTextureTransColG32SemiTrans_CheckMask;
			} else {
				func = GetTextureTransColG32SemiTrans_NoCheckMask; 
			}
		}
	} else {
		if (bCheckMask) {
			func = GetTextureTransColG32NoSemiTrans_CheckMask;
		} else {
			func = GetTextureTransColG32NoSemiTrans_NoCheckMask; 
		}
	}
#endif
	return func;
}



__inline void __GetShadeTransCheckMask(uint32_t * pdest, int32_t r, int32_t g,int32_t b) {
	uint32_t ma=GETLE32(pdest);
	PUTLE32(pdest, (X32PSXCOL(r,g,b))|lSetMask);//0x80008000;
	if(ma&0x80000000) PUTLE32(pdest, (ma&0xFFFF0000)|(*pdest&0xFFFF));
	if(ma&0x00008000) PUTLE32(pdest, (ma&0xFFFF)    |(*pdest&0xFFFF0000));
}
		


__inline void GetShadeTransCol32ABR0_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	PUTLE32(pdest, (((GETLE32(pdest)&0x7bde7bde)>>1)+(((color)&0x7bde7bde)>>1))|lSetMask);//0x80008000;
} 

__inline void GetShadeTransCol32ABR0_CheckMask(uint32_t * pdest, uint32_t color)
{
	int32_t r,g,b;

	r=(X32ACOL1(GETLE32(pdest))>>1)+((X32ACOL1(color))>>1);
	b=(X32ACOL2(GETLE32(pdest))>>1)+((X32ACOL2(color))>>1);
	g=(X32ACOL3(GETLE32(pdest))>>1)+((X32ACOL3(color))>>1);

	__ClampRGB(r, g, b);

	__GetShadeTransCheckMask(pdest, r, g, b);
} 

__inline void GetShadeTransCol32ABR1_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	int32_t r,g,b;

	r=(X32COL1(GETLE32(pdest)))+((X32COL1(color)));
	b=(X32COL2(GETLE32(pdest)))+((X32COL2(color)));
	g=(X32COL3(GETLE32(pdest)))+((X32COL3(color)));

	__ClampRGB(r, g, b);

	PUTLE32(pdest, (X32PSXCOL(r,g,b))|lSetMask);//0x80008000;
}  

__inline void GetShadeTransCol32ABR1_CheckMask(uint32_t * pdest, uint32_t color)
{
	int32_t r,g,b;

	r=(X32COL1(GETLE32(pdest)))+((X32COL1(color)));
	b=(X32COL2(GETLE32(pdest)))+((X32COL2(color)));
	g=(X32COL3(GETLE32(pdest)))+((X32COL3(color)));

	__ClampRGB(r, g, b);

	__GetShadeTransCheckMask(pdest, r, g, b);
}   

__inline void GetShadeTransCol32ABR2_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	int32_t r,g,b;
	int32_t sr,sb,sg,src,sbc,sgc,c;

	src=XCOL1(color);sbc=XCOL2(color);sgc=XCOL3(color);
	c=GETLE32(pdest)>>16;
	sr=(XCOL1(c))-src;   if(sr&0x8000) sr=0;
	sb=(XCOL2(c))-sbc;  if(sb&0x8000) sb=0;
	sg=(XCOL3(c))-sgc; if(sg&0x8000) sg=0;
	r=((int32_t)sr)<<16;b=((int32_t)sb)<<11;g=((int32_t)sg)<<6;
	c=LOWORD(GETLE32(pdest));
	sr=(XCOL1(c))-src;   if(sr&0x8000) sr=0;
	sb=(XCOL2(c))-sbc;  if(sb&0x8000) sb=0;
	sg=(XCOL3(c))-sgc; if(sg&0x8000) sg=0;
	r|=sr;b|=sb>>5;g|=sg>>10;

	__ClampRGB(r, g, b);

	PUTLE32(pdest, (X32PSXCOL(r,g,b))|lSetMask);//0x80008000;
}  

__inline void GetShadeTransCol32ABR2_CheckMask(uint32_t * pdest, uint32_t color)
{
	int32_t r,g,b;
	int32_t sr,sb,sg,src,sbc,sgc,c;

	src=XCOL1(color);sbc=XCOL2(color);sgc=XCOL3(color);
	c=GETLE32(pdest)>>16;
	sr=(XCOL1(c))-src;   if(sr&0x8000) sr=0;
	sb=(XCOL2(c))-sbc;  if(sb&0x8000) sb=0;
	sg=(XCOL3(c))-sgc; if(sg&0x8000) sg=0;
	r=((int32_t)sr)<<16;b=((int32_t)sb)<<11;g=((int32_t)sg)<<6;
	c=LOWORD(GETLE32(pdest));
	sr=(XCOL1(c))-src;   if(sr&0x8000) sr=0;
	sb=(XCOL2(c))-sbc;  if(sb&0x8000) sb=0;
	sg=(XCOL3(c))-sgc; if(sg&0x8000) sg=0;
	r|=sr;b|=sb>>5;g|=sg>>10;

	__ClampRGB(r, g, b);

	__GetShadeTransCheckMask(pdest, r, g, b);
}   

__inline void GetShadeTransCol32SemiTrans_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	int32_t r,g,b;

#ifdef HALFBRIGHTMODE3
	r=(X32COL1(GETLE32(pdest)))+((X32BCOL1(color))>>2);
	b=(X32COL2(GETLE32(pdest)))+((X32BCOL2(color))>>2);
	g=(X32COL3(GETLE32(pdest)))+((X32BCOL3(color))>>2);
#else
	r=(X32COL1(GETLE32(pdest)))+((X32ACOL1(color))>>1);
	b=(X32COL2(GETLE32(pdest)))+((X32ACOL2(color))>>1);
	g=(X32COL3(GETLE32(pdest)))+((X32ACOL3(color))>>1);
#endif

	__ClampRGB(r, g, b);

	PUTLE32(pdest, (X32PSXCOL(r,g,b))|lSetMask);//0x80008000;
} 

__inline void GetShadeTransCol32SemiTrans_CheckMask(uint32_t * pdest, uint32_t color)
{
	int32_t r,g,b;

#ifdef HALFBRIGHTMODE3
	r=(X32COL1(GETLE32(pdest)))+((X32BCOL1(color))>>2);
	b=(X32COL2(GETLE32(pdest)))+((X32BCOL2(color))>>2);
	g=(X32COL3(GETLE32(pdest)))+((X32BCOL3(color))>>2);
#else
	r=(X32COL1(GETLE32(pdest)))+((X32ACOL1(color))>>1);
	b=(X32COL2(GETLE32(pdest)))+((X32ACOL2(color))>>1);
	g=(X32COL3(GETLE32(pdest)))+((X32ACOL3(color))>>1);
#endif

	__ClampRGB(r, g, b);
	
	__GetShadeTransCheckMask(pdest, r, g, b);
} 

__inline void GetShadeTransCol32NoSemiTrans_CheckMask(uint32_t * pdest, uint32_t color)
{
	uint32_t ma=GETLE32(pdest);
	PUTLE32(pdest, color|lSetMask);//0x80008000;
	if(ma&0x80000000) PUTLE32(pdest, (ma&0xFFFF0000)|(GETLE32(pdest)&0xFFFF));
	if(ma&0x00008000) PUTLE32(pdest, (ma&0xFFFF)    |(GETLE32(pdest)&0xFFFF0000));
}  

__inline void GetShadeTransCol32NoSemiTrans_NoCheckMask(uint32_t * pdest, uint32_t color)
{
	PUTLE32(pdest, color|lSetMask);//0x80008000;
}

t_getshadetrans_func GetShadeTransCol32Func() {
	// By default take the slow one ...
	t_getshadetrans_func func = GetShadeTransCol32;
#ifdef USE_NEW_FUNC
	if(DrawSemiTrans) {
		if (GlobalTextABR==0)
		{
			if (bCheckMask) {
				func = GetShadeTransCol32ABR0_CheckMask;
			} else {
				func = GetShadeTransCol32ABR0_NoCheckMask; 
			}
			
		} else if(GlobalTextABR==1) {
			if (bCheckMask) {
				func = GetShadeTransCol32ABR1_CheckMask;
			} else {
				func = GetShadeTransCol32ABR1_NoCheckMask; 
			}
		} else if(GlobalTextABR==2) {
			if (bCheckMask) {
				func = GetShadeTransCol32ABR2_CheckMask;
			} else {
				func = GetShadeTransCol32ABR2_NoCheckMask; 
			}
		} else {
			if (bCheckMask) {
				func = GetShadeTransCol32SemiTrans_CheckMask;
			} else {
				func = GetShadeTransCol32SemiTrans_NoCheckMask; 
			}
		}
	} else {
		if (bCheckMask) {
			func = GetShadeTransCol32NoSemiTrans_CheckMask;
		} else {
			func = GetShadeTransCol32NoSemiTrans_NoCheckMask; 
		}
	}
#endif
	return func;
}
