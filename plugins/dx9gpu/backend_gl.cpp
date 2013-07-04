//#define USE_GL_1
#ifdef USE_GL_1

#include "stdafx.h"
#include "externals.h"

#include <GL/glew.h>
#include <GL/gl.h>
//#include "gl_ext.h"

#include "backend.h"

static unsigned char * pGfxCardScreen=0;

static int iClampType=GL_TO_EDGE_CLAMP;

////////////////////////////////////////////////////////////////////////
// OpenGL primitive drawing commands
////////////////////////////////////////////////////////////////////////
void PRIMdrawTexturedQuad(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2fv(&vertex1->sow);
	glVertex3fv(&vertex1->x);

	glTexCoord2fv(&vertex2->sow);
	glVertex3fv(&vertex2->x);

	glTexCoord2fv(&vertex4->sow);
	glVertex3fv(&vertex4->x);

	glTexCoord2fv(&vertex3->sow);
	glVertex3fv(&vertex3->x);
	glEnd();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTexturedTri(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3) 
{
	glBegin(GL_TRIANGLES);
	glTexCoord2fv(&vertex1->sow);
	glVertex3fv(&vertex1->x);

	glTexCoord2fv(&vertex2->sow);
	glVertex3fv(&vertex2->x);

	glTexCoord2fv(&vertex3->sow);
	glVertex3fv(&vertex3->x);
	glEnd();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTexGouraudTriColor(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3) 
{
	glBegin(GL_TRIANGLES);

	SETPCOL(vertex1); 
	glTexCoord2fv(&vertex1->sow);
	glVertex3fv(&vertex1->x);

	SETPCOL(vertex2); 
	glTexCoord2fv(&vertex2->sow);
	glVertex3fv(&vertex2->x);

	SETPCOL(vertex3); 
	glTexCoord2fv(&vertex3->sow);
	glVertex3fv(&vertex3->x);
	glEnd();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTexGouraudTriColorQuad(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	glBegin(GL_TRIANGLE_STRIP);
	SETPCOL(vertex1); 
	glTexCoord2fv(&vertex1->sow);
	glVertex3fv(&vertex1->x);

	SETPCOL(vertex2); 
	glTexCoord2fv(&vertex2->sow);
	glVertex3fv(&vertex2->x);

	SETPCOL(vertex4); 
	glTexCoord2fv(&vertex4->sow);
	glVertex3fv(&vertex4->x);

	SETPCOL(vertex3); 
	glTexCoord2fv(&vertex3->sow);
	glVertex3fv(&vertex3->x);
	glEnd();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTri(OGLVertex* vertex1, OGLVertex* vertex2, OGLVertex* vertex3) 
{
	glBegin(GL_TRIANGLES);
	glVertex3fv(&vertex1->x);
	glVertex3fv(&vertex2->x);
	glVertex3fv(&vertex3->x);
	glEnd();
}

///////////////////////////////////////////////////////// 

void PRIMdrawTri2(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	glBegin(GL_TRIANGLE_STRIP);                           
	glVertex3fv(&vertex1->x);
	glVertex3fv(&vertex3->x);
	glVertex3fv(&vertex2->x);
	glVertex3fv(&vertex4->x);
	glEnd();
}

///////////////////////////////////////////////////////// 

void PRIMdrawGouraudTriColor(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3) 
{
	glBegin(GL_TRIANGLES);                           
	SETPCOL(vertex1); 
	glVertex3fv(&vertex1->x);

	SETPCOL(vertex2); 
	glVertex3fv(&vertex2->x);

	SETPCOL(vertex3); 
	glVertex3fv(&vertex3->x);
	glEnd();
}

///////////////////////////////////////////////////////// 

void PRIMdrawGouraudTri2Color(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	glBegin(GL_TRIANGLE_STRIP);                           
	SETPCOL(vertex1); 
	glVertex3fv(&vertex1->x);

	SETPCOL(vertex3); 
	glVertex3fv(&vertex3->x);

	SETPCOL(vertex2); 
	glVertex3fv(&vertex2->x);

	SETPCOL(vertex4); 
	glVertex3fv(&vertex4->x);
	glEnd();
}

///////////////////////////////////////////////////////// 

void PRIMdrawFlatLine(OGLVertex* vertex1, OGLVertex* vertex2,OGLVertex* vertex3, OGLVertex* vertex4)
{
	glBegin(GL_QUADS);

	SETPCOL(vertex1); 

	glVertex3fv(&vertex1->x);
	glVertex3fv(&vertex2->x);
	glVertex3fv(&vertex3->x);
	glVertex3fv(&vertex4->x);
	glEnd();
}

///////////////////////////////////////////////////////// 

void PRIMdrawGouraudLine(OGLVertex* vertex1, OGLVertex* vertex2,OGLVertex* vertex3, OGLVertex* vertex4)
{
	glBegin(GL_QUADS);

	SETPCOL(vertex1); 
	glVertex3fv(&vertex1->x);

	SETPCOL(vertex2); 
	glVertex3fv(&vertex2->x);

	SETPCOL(vertex3); 
	glVertex3fv(&vertex3->x);

	SETPCOL(vertex4); 
	glVertex3fv(&vertex4->x);
	glEnd();
}

///////////////////////////////////////////////////////// 

void PRIMdrawQuad(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	glBegin(GL_QUADS);
	glVertex3fv(&vertex1->x);
	glVertex3fv(&vertex2->x);
	glVertex3fv(&vertex3->x);
	glVertex3fv(&vertex4->x);
	glEnd();
}


////////////////////////////////////////////////////////////////////////
// paint it black: simple func to clean up optical border garbage
////////////////////////////////////////////////////////////////////////

void PaintBlackBorders(void)
{
	short s;

	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_TEXTURE_2D);
	glShadeModel(GL_FLAT);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	glBegin(GL_QUADS);

	vertex[0].c.lcol=0xff000000;
	SETCOL(vertex[0]); 

	if(PreviousPSXDisplay.Range.x0)
	{
		s=PreviousPSXDisplay.Range.x0+1;
		glVertex3f(0,0,0.99996f);
		glVertex3f(0,PSXDisplay.DisplayMode.y,0.99996f);
		glVertex3f(s,PSXDisplay.DisplayMode.y,0.99996f);
		glVertex3f(s,0,0.99996f);

		s+=PreviousPSXDisplay.Range.x1-2;

		glVertex3f(s,0,0.99996f);
		glVertex3f(s,PSXDisplay.DisplayMode.y,0.99996f);
		glVertex3f(PSXDisplay.DisplayMode.x,PSXDisplay.DisplayMode.y,0.99996f);
		glVertex3f(PSXDisplay.DisplayMode.x,0,0.99996f);
	}

	if(PreviousPSXDisplay.Range.y0)
	{
		s=PreviousPSXDisplay.Range.y0+1;
		glVertex3f(0,0,0.99996f);
		glVertex3f(0,s,0.99996f);
		glVertex3f(PSXDisplay.DisplayMode.x,s,0.99996f);
		glVertex3f(PSXDisplay.DisplayMode.x,0,0.99996f);
	}

	glEnd();

	glEnable(GL_ALPHA_TEST);
	glEnable(GL_SCISSOR_TEST);
}


////////////////////////////////////////////////////////////////////////
// Aspect ratio of ogl screen: simply adjusting ogl view port
////////////////////////////////////////////////////////////////////////

void SetAspectRatio(void)
{
	float xs,ys,s;RECT r;

	if(!PSXDisplay.DisplayModeNew.x) return;
	if(!PSXDisplay.DisplayModeNew.y) return;

	xs=(float)iResX/(float)PSXDisplay.DisplayModeNew.x;
	ys=(float)iResY/(float)PSXDisplay.DisplayModeNew.y;

	s=min(xs,ys);
	r.right =(int)((float)PSXDisplay.DisplayModeNew.x*s);
	r.bottom=(int)((float)PSXDisplay.DisplayModeNew.y*s);
	if(r.right  > iResX) r.right  = iResX;
	if(r.bottom > iResY) r.bottom = iResY;
	if(r.right  < 1)     r.right  = 1;
	if(r.bottom < 1)     r.bottom = 1;

	r.left = (iResX-r.right)/2;
	r.top  = (iResY-r.bottom)/2;

	if(r.bottom<rRatioRect.bottom ||
		r.right <rRatioRect.right)
	{
		RECT rC;
		glClearColor(0,0,0,128);                         

		if(r.right <rRatioRect.right)
		{
			rC.left=0;
			rC.top=0;
			rC.right=r.left;
			rC.bottom=iResY;
			glScissor(rC.left,rC.top,rC.right,rC.bottom);
			glClear(uiBufferBits);
			rC.left=iResX-rC.right;
			glScissor(rC.left,rC.top,rC.right,rC.bottom);
			glClear(uiBufferBits);
		}

		if(r.bottom <rRatioRect.bottom)
		{
			rC.left=0;
			rC.top=0;
			rC.right=iResX;
			rC.bottom=r.top;
			glScissor(rC.left,rC.top,rC.right,rC.bottom);
			glClear(uiBufferBits);
			rC.top=iResY-rC.bottom;
			glScissor(rC.left,rC.top,rC.right,rC.bottom);
			glClear(uiBufferBits);
		}

		bSetClip=TRUE;
		bDisplayNotSet=TRUE;
	}

	rRatioRect=r;


	glViewport(rRatioRect.left,
		iResY-(rRatioRect.top+rRatioRect.bottom),
		rRatioRect.right,
		rRatioRect.bottom);                         // init viewport
}



////////////////////////////////////////////////////////////////////////
// vram read check ex (reading from card's back/frontbuffer if needed...
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamReadEx(int x, int y, int dx, int dy)
{
	unsigned short sArea;
	int ux,uy,udx,udy,wx,wy;
	unsigned short * p1, *p2;
	float XS,YS;
	unsigned char * ps;
	unsigned char * px;
	unsigned short s,sx;

	if(STATUSREG&GPUSTATUS_RGB24) return;

	if(((dx  > PSXDisplay.DisplayPosition.x) &&
		(x   < PSXDisplay.DisplayEnd.x) &&
		(dy  > PSXDisplay.DisplayPosition.y) &&
		(y   < PSXDisplay.DisplayEnd.y)))
		sArea=0;
	else
		if((!(PSXDisplay.InterlacedTest) &&
			(dx  > PreviousPSXDisplay.DisplayPosition.x) &&
			(x   < PreviousPSXDisplay.DisplayEnd.x) &&
			(dy  > PreviousPSXDisplay.DisplayPosition.y) &&
			(y   < PreviousPSXDisplay.DisplayEnd.y)))
			sArea=1;
		else 
		{
			return;
		}

		//////////////

		if(iRenderFVR)
		{
			bFullVRam=TRUE;iRenderFVR=2;return;
		}
		bFullVRam=TRUE;iRenderFVR=2;

		//////////////

		p2=0;

		if(sArea==0)
		{
			ux=PSXDisplay.DisplayPosition.x;
			uy=PSXDisplay.DisplayPosition.y;
			udx=PSXDisplay.DisplayEnd.x-ux;
			udy=PSXDisplay.DisplayEnd.y-uy;
			if((PreviousPSXDisplay.DisplayEnd.x-
				PreviousPSXDisplay.DisplayPosition.x)==udx &&
				(PreviousPSXDisplay.DisplayEnd.y-
				PreviousPSXDisplay.DisplayPosition.y)==udy)
				p2=(psxVuw + (1024*PreviousPSXDisplay.DisplayPosition.y) + 
				PreviousPSXDisplay.DisplayPosition.x);
		}
		else
		{
			ux=PreviousPSXDisplay.DisplayPosition.x;
			uy=PreviousPSXDisplay.DisplayPosition.y;
			udx=PreviousPSXDisplay.DisplayEnd.x-ux;
			udy=PreviousPSXDisplay.DisplayEnd.y-uy;
			if((PSXDisplay.DisplayEnd.x-
				PSXDisplay.DisplayPosition.x)==udx &&
				(PSXDisplay.DisplayEnd.y-
				PSXDisplay.DisplayPosition.y)==udy)
				p2=(psxVuw + (1024*PSXDisplay.DisplayPosition.y) + 
				PSXDisplay.DisplayPosition.x);
		}

		p1=(psxVuw + (1024*uy) + ux);
		if(p1==p2) p2=0;

		x=0;y=0;
		wx=dx=udx;wy=dy=udy;

		if(udx<=0) return;
		if(udy<=0) return;
		if(dx<=0)  return;
		if(dy<=0)  return;
		if(wx<=0)  return;
		if(wy<=0)  return;

		XS=(float)rRatioRect.right/(float)wx;
		YS=(float)rRatioRect.bottom/(float)wy;

		dx=(int)((float)(dx)*XS);
		dy=(int)((float)(dy)*YS);

		if(dx>iResX) dx=iResX;
		if(dy>iResY) dy=iResY;

		if(dx<=0) return;
		if(dy<=0) return;

		// ogl y adjust
		y=iResY-y-dy;

		x+=rRatioRect.left;
		y-=rRatioRect.top;

		if(y<0) y=0; if((y+dy)>iResY) dy=iResY-y;

		if(!pGfxCardScreen)
		{
			glPixelStorei(GL_PACK_ALIGNMENT,1);
			pGfxCardScreen=(unsigned char *)malloc(iResX*iResY*4);
		}

		ps=pGfxCardScreen;

		if(!sArea) glReadBuffer(GL_FRONT);

		glReadPixels(x,y,dx,dy,GL_RGB,GL_UNSIGNED_BYTE,ps);

		if(!sArea) glReadBuffer(GL_BACK);

		s=0;

		XS=(float)dx/(float)(udx);
		YS=(float)dy/(float)(udy+1);

		for(y=udy;y>0;y--)
		{
			for(x=0;x<udx;x++)
			{
				if(p1>=psxVuw && p1<psxVuw_eom)
				{
					px=ps+(3*((int)((float)x * XS))+
						(3*dx)*((int)((float)y*YS)));
					sx=(*px)>>3;px++;
					s=sx;
					sx=(*px)>>3;px++;
					s|=sx<<5;
					sx=(*px)>>3;
					s|=sx<<10;
					s&=~0x8000;
					*p1=s;
				}
				if(p2>=psxVuw && p2<psxVuw_eom) *p2=s;

				p1++;
				if(p2) p2++;
			}

			p1 += 1024 - udx;
			if(p2) p2 += 1024 - udx;
		}
}

////////////////////////////////////////////////////////////////////////
// vram read check (reading from card's back/frontbuffer if needed... 
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamRead(int x, int y, int dx, int dy,BOOL bFront)
{
	unsigned short sArea;unsigned short * p;
	int ux,uy,udx,udy,wx,wy;float XS,YS;
	unsigned char * ps, * px;
	unsigned short s=0,sx;

	if(STATUSREG&GPUSTATUS_RGB24) return;

	if(((dx  > PSXDisplay.DisplayPosition.x) &&
		(x   < PSXDisplay.DisplayEnd.x) &&
		(dy  > PSXDisplay.DisplayPosition.y) &&
		(y   < PSXDisplay.DisplayEnd.y)))
		sArea=0;
	else
		if((!(PSXDisplay.InterlacedTest) &&
			(dx  > PreviousPSXDisplay.DisplayPosition.x) &&
			(x   < PreviousPSXDisplay.DisplayEnd.x) &&
			(dy  > PreviousPSXDisplay.DisplayPosition.y) &&
			(y   < PreviousPSXDisplay.DisplayEnd.y)))
			sArea=1;
		else 
		{
			return;
		}

		if(dwActFixes&0x40)
		{
			if(iRenderFVR)
			{
				bFullVRam=TRUE;iRenderFVR=2;return;
			}
			bFullVRam=TRUE;iRenderFVR=2;
		}

		ux=x;uy=y;udx=dx;udy=dy;

		if(sArea==0)
		{
			x -=PSXDisplay.DisplayPosition.x;
			dx-=PSXDisplay.DisplayPosition.x;
			y -=PSXDisplay.DisplayPosition.y;
			dy-=PSXDisplay.DisplayPosition.y;
			wx=PSXDisplay.DisplayEnd.x-PSXDisplay.DisplayPosition.x;
			wy=PSXDisplay.DisplayEnd.y-PSXDisplay.DisplayPosition.y;
		}
		else
		{
			x -=PreviousPSXDisplay.DisplayPosition.x;
			dx-=PreviousPSXDisplay.DisplayPosition.x;
			y -=PreviousPSXDisplay.DisplayPosition.y;
			dy-=PreviousPSXDisplay.DisplayPosition.y;
			wx=PreviousPSXDisplay.DisplayEnd.x-PreviousPSXDisplay.DisplayPosition.x;
			wy=PreviousPSXDisplay.DisplayEnd.y-PreviousPSXDisplay.DisplayPosition.y;
		}
		if(x<0) {ux-=x;x=0;}
		if(y<0) {uy-=y;y=0;}
		if(dx>wx) {udx-=(dx-wx);dx=wx;}
		if(dy>wy) {udy-=(dy-wy);dy=wy;}
		udx-=ux;
		udy-=uy;

		p=(psxVuw + (1024*uy) + ux);

		if(udx<=0) return;
		if(udy<=0) return;
		if(dx<=0)  return;
		if(dy<=0)  return;
		if(wx<=0)  return;
		if(wy<=0)  return;

		XS=(float)rRatioRect.right/(float)wx;
		YS=(float)rRatioRect.bottom/(float)wy;

		dx=(int)((float)(dx)*XS);
		dy=(int)((float)(dy)*YS);
		x=(int)((float)x*XS);
		y=(int)((float)y*YS);

		dx-=x;
		dy-=y;

		if(dx>iResX) dx=iResX;
		if(dy>iResY) dy=iResY;

		if(dx<=0) return;
		if(dy<=0) return;

		// ogl y adjust
		y=iResY-y-dy;

		x+=rRatioRect.left;
		y-=rRatioRect.top;

		if(y<0) y=0; if((y+dy)>iResY) dy=iResY-y;

		if(!pGfxCardScreen)
		{
			glPixelStorei(GL_PACK_ALIGNMENT,1);
			pGfxCardScreen=(unsigned char *)malloc(iResX*iResY*4);
		}

		ps=pGfxCardScreen;

		if(bFront) glReadBuffer(GL_FRONT);

		glReadPixels(x,y,dx,dy,GL_RGB,GL_UNSIGNED_BYTE,ps);

		if(bFront) glReadBuffer(GL_BACK);

		XS=(float)dx/(float)(udx);
		YS=(float)dy/(float)(udy+1);

		for(y=udy;y>0;y--)
		{
			for(x=0;x<udx;x++)
			{
				if(p>=psxVuw && p<psxVuw_eom)
				{
					px=ps+(3*((int)((float)x * XS))+
						(3*dx)*((int)((float)y*YS)));
					sx=(*px)>>3;px++;
					s=sx;
					sx=(*px)>>3;px++;
					s|=sx<<5;
					sx=(*px)>>3;
					s|=sx<<10;
					s&=~0x8000;
					*p=s;
				}
				p++;
			}
			p += 1024 - udx;
		}
}

////////////////////////////////////////////////////////////////////////
void BK_SetView(int w, int h) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, h, 0, -1, 1);
}

////////////////////////////////////////////////////////////////////////
#ifdef _WINDOWS
extern HGLRC    GLCONTEXT;
#endif

void BK_SwapBuffer() {	
#ifdef _WINDOWS
	{                                                    // windows: 
		HDC hdc=GetDC(hWWindow);
		wglMakeCurrent(hdc, GLCONTEXT);                      // -> make current again
		SwapBuffers(wglGetCurrentDC());                    // -> swap
		ReleaseDC(hWWindow,hdc);                            // -> ! important !
	}
#elif defined (_MACGL)
	DoBufferSwap();
#else
	glXSwapBuffers(display,window);
#endif
}

////////////////////////////////////////////////////////////////////////
void BK_Clear(uint32_t flags, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	glClearColor(((float)r/255.f), ((float)g/255.f), ((float)b/255.f), ((float)a/255.f));
	glClear(flags);
}


void BK_ClearDepth() {                 
	glClear(GL_DEPTH_BUFFER_BIT);
}

////////////////////////////////////////////////////////////////////////
void BK_OpaqueOn() {
	glAlphaFunc(GL_EQUAL, 0.0f);
	glDisable(GL_BLEND);
}

void BK_OpaqueOff() {
	glAlphaFunc(GL_GREATER, 0.49f);
}

////////////////////////////////////////////////////////////////////////
void BK_ScissorEnable() {
	glEnable(GL_SCISSOR_TEST);
}
void BK_ScissorDisable()  {	
	glDisable(GL_SCISSOR_TEST); 
}
void BK_Scissor(int l, int t, int r, int b)  {
	b -=t;
	t = iResY - ( t + b);
	glScissor(l, t, r - l, b);
}

////////////////////////////////////////////////////////////////////////
void BK_AlphaEnable() {
	glEnable(GL_ALPHA_TEST); 
}

void BK_AlphaDisable() {
	glDisable(GL_ALPHA_TEST); 
}

////////////////////////////////////////////////////////////////////////
void BK_DepthEnable() {	
	uiBufferBits=GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT;

	glEnable(GL_DEPTH_TEST);    
	glDepthFunc(GL_ALWAYS);
}
void BK_DepthMask() {
	glDepthFunc(GL_LESS);
}
void BK_DepthUnmask() {
	glDepthFunc(GL_ALWAYS);
}
void BK_DepthDisable() {
	uiBufferBits=GL_COLOR_BUFFER_BIT;

	glDisable(GL_DEPTH_TEST);    
}


////////////////////////////////////////////////////////////////////////                                          
// Transparent blending settings
////////////////////////////////////////////////////////////////////////

static GLenum obm1=GL_ZERO;
static GLenum obm2=GL_ZERO;

typedef struct SEMITRANSTAG
{
	GLenum  srcFac;
	GLenum  dstFac;
	unsigned char alpha;
} SemiTransParams;

static SemiTransParams TransSets[4]=
{
	{GL_SRC_ALPHA,GL_SRC_ALPHA,          127},
	{GL_ONE,      GL_ONE,                255},
	{GL_ZERO,     GL_ONE_MINUS_SRC_COLOR,255},
	{GL_ONE_MINUS_SRC_ALPHA,GL_ONE,      192}
}; 

////////////////////////////////////////////////////////////////////////

void SetSemiTrans(void)
{
	/*
	* 0.5 x B + 0.5 x F
	* 1.0 x B + 1.0 x F
	* 1.0 x B - 1.0 x F
	* 1.0 x B +0.25 x F
	*/

	// no semi trans at all?
	if(!DrawSemiTrans)                                    
	{

		// -> don't wanna blend
		glDisable(GL_BLEND);
		ubGloAlpha=ubGloColAlpha=255;                       // -> full alpha
		return;                                             // -> and bye
	}

	ubGloAlpha=ubGloColAlpha=TransSets[GlobalTextABR].alpha;
	// wanna blend
	glEnable(GL_BLEND);

	if(TransSets[GlobalTextABR].srcFac!=obm1 || 
		TransSets[GlobalTextABR].dstFac!=obm2)
	{
		if(TransSets[GlobalTextABR].dstFac !=GL_ONE_MINUS_SRC_COLOR)
		{
			if(obm2==GL_ONE_MINUS_SRC_COLOR)
				glBlendEquation(GL_FUNC_ADD);
			obm1=TransSets[GlobalTextABR].srcFac;
			obm2=TransSets[GlobalTextABR].dstFac;
			glBlendFunc(obm1,obm2);                           // set blend func
		}
		else
		{
			glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
			obm1=TransSets[GlobalTextABR].srcFac;
			obm2=TransSets[GlobalTextABR].dstFac;
			glBlendFunc(GL_ONE,GL_ONE);                       // set blend func
			//glBlendFunc(obm1,obm2);                           // set blend func
		}
	}
}


////////////////////////////////////////////////////////////////////////

void BK_SetColor(unsigned char * c) {
	glColor4ubv(c);
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
#define DEFAULT_TEXTURE_SIZE 1024 * 1024 * 4

static void * gl_texture_buffer = NULL;
struct BK_Texture{
	unsigned int tex;
	void * data;
	int pitch;
};


void BK_InitTextures() {
	gl_texture_buffer = malloc(DEFAULT_TEXTURE_SIZE);
}

bk_texture_t * BK_CreateTexture(int width, int height, int unused) {
	bk_texture_t * texture = new bk_texture_t;
	texture->_private = new BK_Texture;
	texture->_private->tex = -1;
	glGenTextures(1, &texture->_private->tex);


	glBindTexture(GL_TEXTURE_2D, texture->_private->tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iClampType);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, iClampType);

	// Fake call (needed to create the texture)
	BK_SetTextureData(texture, width, height, gl_texture_buffer);

	return texture;
}

void BK_SetTexture(bk_texture_t * texture) {
	if (texture && texture->_private) {
		glBindTexture(GL_TEXTURE_2D, texture->_private->tex);
	} else {
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void BK_DeleteTexture(bk_texture_t * texture) {
	glDeleteTextures(1, &texture->_private->tex);
	delete texture->_private;
	delete texture;
}

int BK_SetTextureData(bk_texture_t * texture, int texw, int texh, void * data) {
	glBindTexture(GL_TEXTURE_2D, texture->_private->tex);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texw, texh, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	return 0;
}

int BK_SetSubTextureData(bk_texture_t * texture, int xoffset, int yoffset, int texw, int texh, void * data) {
	glBindTexture(GL_TEXTURE_2D, texture->_private->tex);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 
		xoffset, yoffset,
		texw, texh,
		GL_RGBA, GL_UNSIGNED_BYTE, data
		);

	return 0;
}

unsigned int BK_GetGlTexture(bk_texture_t * texture) {
	return texture->_private->tex;
}


void BK_TextureEnable() {
	glEnable(GL_TEXTURE_2D);
}

void BK_TextureDisable() {	
	glBindTexture(GL_TEXTURE_2D,0);
	glDisable(GL_TEXTURE_2D);
}
#ifdef _WINDOWS
////////////////////////////////////////////////////////////////////////
HDC            dcGlobal = NULL;
HWND           hWWindow;

BOOL bSetupPixelFormat(HDC hDC)
{
	int pixelformat;
	static PIXELFORMATDESCRIPTOR pfd = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),    // size of this pfd
		1,                               // version number
		PFD_DRAW_TO_WINDOW |             // support window
		PFD_SUPPORT_OPENGL |           // support OpenGL
		PFD_DOUBLEBUFFER,              // double buffered
		PFD_TYPE_RGBA,                   // RGBA type
		16,                              // 16-bit color depth  (adjusted later)
		0, 0, 0, 0, 0, 0,                // color bits ignored
		0,                               // no alpha buffer
		0,                               // shift bit ignored
		0,                               // no accumulation buffer
		0, 0, 0, 0,                      // accum bits ignored
		0,                               // z-buffer    
		0,
		0,                               // no auxiliary buffer
		PFD_MAIN_PLANE,                  // main layer
		0,                               // reserved
		0, 0, 0                          // layer masks ignored
	};

	pfd.cColorBits=iColDepth;                             // set user color depth
	pfd.cDepthBits=iZBufferDepth;                         // set user zbuffer (by psx mask)

	if((pixelformat=ChoosePixelFormat(hDC,&pfd))==0)     
	{
		MessageBoxA(NULL,"ChoosePixelFormat failed","Error",MB_OK);
		return FALSE;
	}

	if(SetPixelFormat(hDC,pixelformat, &pfd)==FALSE)
	{
		MessageBoxA(NULL,"SetPixelFormat failed","Error",MB_OK);
		return FALSE;
	}

	return TRUE;
}
#endif

void BK_Init() {
#ifdef _WINDOWS
	HGLRC objectRC;
	// init
	dcGlobal = GetDC(hWWindow);                           // FIRST: dc/rc stuff

	bSetupPixelFormat(dcGlobal);

	objectRC = wglCreateContext(dcGlobal); 
	GLCONTEXT=objectRC;
	wglMakeCurrent(dcGlobal, objectRC);

	// CheckWGLExtensions(dcGlobal);
	if(bWindowMode) ReleaseDC(hWWindow,dcGlobal);         // win mode: release dc again

	MoveWindow(hWWindow,                                // -> center wnd
		GetSystemMetrics(SM_CXFULLSCREEN)/2-iResX/2,
		GetSystemMetrics(SM_CYFULLSCREEN)/2-iResY/2,
		iResX+GetSystemMetrics(SM_CXFIXEDFRAME)+3,
		iResY+GetSystemMetrics(SM_CYFIXEDFRAME)+GetSystemMetrics(SM_CYCAPTION)+3,
		TRUE);

#endif
	
	GLenum err = glewInit();

	//----------------------------------------------------//

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);    
	glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);     
	glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);     
	glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
	
	glViewport(rRatioRect.left,                           // init viewport by ratio rect
	           iResY-(rRatioRect.top+rRatioRect.bottom),
	           rRatioRect.right,
	           rRatioRect.bottom);

	glDisable(GL_BLEND);
}

void BK_Exit() {

#ifdef _WINDOWS 
	wglMakeCurrent(NULL, NULL);                           // bye context
	if(GLCONTEXT) wglDeleteContext(GLCONTEXT);
	if(!bWindowMode && dcGlobal) 
		ReleaseDC(hWWindow,dcGlobal);
#endif
}


bk_texture_t * gTexName = NULL;
bk_texture_t * gTexFrameName = NULL;


#endif