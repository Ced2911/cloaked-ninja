#define USE_DX9

#ifdef USE_DX9
#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif
#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9.h>

#include "stdafx.h"
#include "externals.h"

#include "backend.h"
#include "backend_dx9_shader.h"
#define USE_SHADERS	1
//#define USE_BUFFER	1

#ifndef _XBOX
LPDIRECT3D9         g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9   g_pd3dDevice = NULL; // Our rendering device
#else
extern LPDIRECT3D9         g_pD3D; // Used to create the D3DDevice
extern LPDIRECT3DDEVICE9   g_pd3dDevice; // Our rendering device
#endif

HWND           hWWindow;

static unsigned char * pGfxCardScreen=0;

static int textured = 0;
static int iClampType=0;

// Fixed pipeline
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_DIFFUSE)

#define MAX_VERTICES 0x10000

// A structure for our custom vertex type. We added texture coordinates
struct CUSTOMVERTEX
{
    float x, y, z;
    unsigned int color;
	float u, v, au, av;
};


static void Draw(int forced = 0);

class dx9states{
private:
	D3DVIEWPORT9 vp;
	RECT scissor;
	RECT fullscreen_scissor;
	
	LPDIRECT3DTEXTURE9 texture;
	int shader;


	template<D3DRENDERSTATETYPE cap, bool init>
	class BoolState {
		bool _value;
	public:
		BoolState() : _value(init) {}

		inline void set(bool value) {
			if(value != _value) {
				Draw(1);
				_value = value;
			}
		}
		inline void enable() {
			set(true);
		}
		inline void disable() {
			set(false);
		}
		operator bool() const {
			return isset();
		}
		inline bool isset() {
			return _value;
		}
		void apply() {
			if(_value)
				g_pd3dDevice->SetRenderState(cap, true);
			else
				g_pd3dDevice->SetRenderState(cap, false);
		}
	};

#define STATE1(state, p1def) \
	class SavedState1_##state { \
		DWORD p1; \
	public: \
		SavedState1_##state() : p1(p1def) {}; \
		void set(DWORD newp1) { \
			if(newp1 != p1) { \
				Draw(1); \
				p1 = newp1; \
			} \
		} \
		void apply() { \
			g_pd3dDevice->SetRenderState(state, p1); \
		} \
	}

#define STATE2(state1, state2, p1def, p2def) \
	class SavedState2_##state1 { \
		DWORD p1; \
		DWORD p2; \
	public: \
		SavedState2_##state1() : p1(p1def), p2(p2def) {}; \
		inline void set(DWORD newp1, DWORD newp2) { \
			if(newp1 != p1 || newp2 != p2) { \
				Draw(1); \
				p1 = newp1; \
				p2 = newp2; \
			} \
		} \
		inline void apply() { \
			g_pd3dDevice->SetRenderState(state1, p1); \
			g_pd3dDevice->SetRenderState(state2, p2); \
		} \
	}

	bool initialized;
	float textureSize[4];
public:
	
	dx9states() : initialized(false) {}

	void SetTextureSize(int smalltexw, int smalltexh, int bigtexw, int bigtexh) {
		textureSize[0] = smalltexw;
		textureSize[1] = smalltexh;
		textureSize[2] = bigtexw;
		textureSize[3] = bigtexh;
	}

	void Initialize() {
		Apply();
		scissor.right = iResX;
		scissor.bottom = iResY;
		fullscreen_scissor.right = iResX;
		fullscreen_scissor.bottom = iResY;

		texture = NULL;
		shader = -1;
	}
	void Apply() {
		blend.apply();
		blendOp.apply();
		blendFunc.apply();
		blendColor.apply();

		alphaTest.apply();
		alphaFunc.apply();
		alphaRef.apply();		
		
		scissorTest.apply();
		if (scissorTest.isset()) {
			g_pd3dDevice->SetScissorRect(&scissor);
		}

		depthTest.apply();
		depthFunc.apply();
		depthWrite.apply();

		stencilEnable.apply();
		stencilFunc.apply();
		stencilPass.apply();
		stencilFail.apply();
		
		SetShaders(shader);
		//g_pd3dDevice->SetTexture(0, texture);
		g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

		g_pd3dDevice->SetVertexShaderConstantF(4, textureSize, 1);
	}

	// Blend
	BoolState<D3DRS_ALPHABLENDENABLE, false> blend;
	STATE2(D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA) blendFunc;
	STATE1(D3DRS_BLENDOP, D3DBLENDOP_ADD) blendOp;
	STATE1(D3DRS_BLENDFACTOR, 0xFFFFFFFF) blendColor;

	// Depth
	BoolState<D3DRS_ZENABLE, false> depthTest;
	STATE1(D3DRS_ZFUNC, D3DCMP_LESS) depthFunc;
	STATE1(D3DRS_ZWRITEENABLE, true) depthWrite;

	// Stencil	
	BoolState<D3DRS_STENCILENABLE, false> stencilEnable;
	STATE1(D3DRS_STENCILFUNC, D3DCMP_ALWAYS) stencilFunc;
	STATE1(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP) stencilPass;
	STATE1(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP) stencilFail;

	// Scissor
	BoolState<D3DRS_SCISSORTESTENABLE, false> scissorTest;
	
	// Viewport
	void setScissorRect(CONST RECT *pRect) {
		if (!EqualRect(&scissor, pRect)) {
			Draw(1);
			memcpy(&scissor, pRect, sizeof(RECT));
		}
	}
	// Alpha
	STATE1(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS) alphaFunc;
	STATE1(D3DRS_ALPHAREF, 0) alphaRef;
	BoolState<D3DRS_ALPHATESTENABLE, false> alphaTest;

	void setTexture(LPDIRECT3DTEXTURE9 tex) {
		if (texture != tex) {
			Draw(1);
			texture = tex;
		}
	}

	void setShader(int s) {
		if (s != shader) {
			Draw(1);
			shader = s;
		}
	}

	void depthRange(float nearz, float farz) {
		vp.MinZ = nearz;
		vp.MaxZ = farz;
	}

	void viewPort(int x, int y, int w, int h) {
		vp.X = x;
		vp.Y = y;
		vp.Width = w;
		vp.Height = h;
	}
} dxstates;

static D3DCOLOR verticesColor;

#ifndef USE_BUFFER
static unsigned int indicesIb[MAX_VERTICES * 3];
static CUSTOMVERTEX dxVerticesVb[MAX_VERTICES * 3];
#else

static LPDIRECT3DINDEXBUFFER9 ib = NULL;
static LPDIRECT3DVERTEXBUFFER9 vb = NULL;

static unsigned int * indicesIb = NULL;
static CUSTOMVERTEX * dxVerticesVb = NULL;


#endif
static CUSTOMVERTEX * dxVertices = dxVerticesVb;
static unsigned int * indices = indicesIb;

static int prevVerticesCount = 0;
static int prevIndicesCount = 0;
static int verticesCount = 0;
static int indicesCount = 0;

static int CurrentPrim = -1;

enum prim_type {
	PRIM_QUAD,
	PRIM_TRI,
	PRIM_TRI_STRIP
};

static unsigned char iAtlasU = 0;
static unsigned char iAtlasV = 0;
static unsigned int texture_size = 256;

static void SetPrimType(int prim_type) {
	if (CurrentPrim != prim_type) {
		//Draw(1);
	}
	CurrentPrim = prim_type;

	//Draw(1);
}

void addIndicesTriangle() {
	SetPrimType(PRIM_TRI);

	*indices++ = verticesCount + 0;
	*indices++ = verticesCount + 1;
	*indices++ = verticesCount + 2;

	indicesCount+=3;
}

void addIndicesTriangleStrip() {
	SetPrimType(PRIM_TRI_STRIP);

	*indices++ = verticesCount + 0;
	*indices++ = verticesCount + 1;
	*indices++ = verticesCount + 2;
	*indices++ = verticesCount + 2;
	*indices++ = verticesCount + 1;
	*indices++ = verticesCount + 3;

	indicesCount+=6;
}


void addIndicesQuad() {
	SetPrimType(PRIM_QUAD);

	*indices++ = verticesCount + 0;
	*indices++ = verticesCount + 1;
	*indices++ = verticesCount + 2;
	*indices++ = verticesCount + 0;
	*indices++ = verticesCount + 2;
	*indices++ = verticesCount + 3;

	indicesCount+=6;
}

extern BOOL           bDrawTextured;                          // current active drawing states
extern BOOL           bDrawSmoothShaded;

void addVertice(OGLVertex * vert) {
	unsigned int c;
	c = D3DCOLOR_ARGB( vert->c.a, vert->c.r, vert->c.g, vert->c.b );

	dxVertices[0].x = vert->x;
	dxVertices[0].y = vert->y;
	//dxVertices[0].z = 1 - vert->z; // GL coord to directx
	dxVertices[0].z = vert->z; // GL coord to directx
	dxVertices[0].color = c;
	dxVertices[0].u = vert->sow;
	dxVertices[0].v = vert->tow;
	dxVertices[0].au = iAtlasU;
	dxVertices[0].av = iAtlasV;

	dxVertices++;
	verticesCount++;
}

static void Draw(int forced) {
	// Vertices batch check
	if ((verticesCount - prevVerticesCount) == 0) {
		return;
	}
	if (forced == 0) {
		return;
	}
	
	dxstates.Apply();
#ifndef USE_BUFFER
	g_pd3dDevice->DrawIndexedPrimitiveUP(
		D3DPT_TRIANGLELIST, 
		0, verticesCount,
		indicesCount/3, 
		indicesIb,
		D3DFMT_INDEX32,
		dxVerticesVb,
		sizeof(CUSTOMVERTEX));
		
	verticesCount = 0;
	indicesCount = 0;

	indices = indicesIb;
	dxVertices = dxVerticesVb;
#else
	g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, (verticesCount), prevIndicesCount, (indicesCount - prevIndicesCount)/3);

	prevVerticesCount = verticesCount;	
	prevIndicesCount = indicesCount;
#endif
}

void BK_UpdateShaders() {	
#ifdef USE_SHADERS
	int type = PS_C;
	if (bDrawTextured) {
		type = PS_G;
	}
	dxstates.setShader(type);
	//dxstates.setShader(PS_C);
#else
	g_pd3dDevice->SetFVF( D3DFVF_CUSTOMVERTEX ); 
#endif
}


////////////////////////////////////////////////////////////////////////
// OpenGL primitive drawing commands
////////////////////////////////////////////////////////////////////////
void PRIMdrawTexturedQuad(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	addIndicesTriangleStrip();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex4);
	addVertice(vertex3);

	Draw();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTexturedTri(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3) 
{
	addIndicesTriangle();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex3);

	Draw();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTexGouraudTriColor(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3) 
{
	addIndicesTriangle();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex3);

	Draw();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTexGouraudTriColorQuad(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	addIndicesTriangleStrip();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex4);
	addVertice(vertex3);

	Draw();
}

///////////////////////////////////////////////////////// 
void PRIMdrawTri(OGLVertex* vertex1, OGLVertex* vertex2, OGLVertex* vertex3) 
{
	addIndicesTriangle();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex3);

	Draw();
}

///////////////////////////////////////////////////////// 

void PRIMdrawTri2(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	addIndicesTriangleStrip();
	addVertice(vertex1);
	addVertice(vertex3);
	addVertice(vertex2);
	addVertice(vertex4);

	Draw();
}

///////////////////////////////////////////////////////// 

void PRIMdrawGouraudTriColor(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3) 
{
	addIndicesTriangle();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex3);

	Draw();
}

///////////////////////////////////////////////////////// 

void PRIMdrawGouraudTri2Color(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	addIndicesTriangleStrip();
	addVertice(vertex1);
	addVertice(vertex3);
	addVertice(vertex2);
	addVertice(vertex4);

	Draw();
}

///////////////////////////////////////////////////////// 

void PRIMdrawFlatLine(OGLVertex* vertex1, OGLVertex* vertex2,OGLVertex* vertex3, OGLVertex* vertex4)
{
	addIndicesQuad();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex3);
	addVertice(vertex4);

	Draw();
}

///////////////////////////////////////////////////////// 

void PRIMdrawGouraudLine(OGLVertex* vertex1, OGLVertex* vertex2,OGLVertex* vertex3, OGLVertex* vertex4)
{
	addIndicesQuad();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex3);
	addVertice(vertex4);

	Draw();
}

///////////////////////////////////////////////////////// 

void PRIMdrawQuad(OGLVertex* vertex1, OGLVertex* vertex2, 
	OGLVertex* vertex3, OGLVertex* vertex4) 
{
	addIndicesQuad();
	addVertice(vertex1);
	addVertice(vertex2);
	addVertice(vertex3);
	addVertice(vertex4);

	Draw();
}


////////////////////////////////////////////////////////////////////////
// paint it black: simple func to clean up optical border garbage
////////////////////////////////////////////////////////////////////////

void PaintBlackBorders(void)
{
	short s;
	unsigned int color;

	// If we have something before
	Draw(1);

	BK_SetTexture(0);
	
	dxstates.scissorTest.set(0);
	dxstates.blend.set(0);
	dxstates.alphaTest.set(0);

	color = D3DCOLOR_ARGB(0xFF, 0, 0, 0);

	if(PreviousPSXDisplay.Range.x0)
	{
		s=PreviousPSXDisplay.Range.x0+1;
		
		dxVertices[0].x = 0;
		dxVertices[0].y = 0;
		dxVertices[0].z = 0.00004f;
		dxVertices[0].color = color;

		dxVertices[1].x = s;
		dxVertices[1].y = PSXDisplay.DisplayMode.y;
		dxVertices[1].z = 0.00004f;
		dxVertices[1].color = color;

		dxVertices[2].x = s;
		dxVertices[2].y = PSXDisplay.DisplayMode.y;
		dxVertices[2].z = 0.00004f;
		dxVertices[2].color = color;

		dxVertices[3].x = 0;
		dxVertices[3].y = 0;
		dxVertices[3].z = 0.00004f;
		dxVertices[3].color = color;

		addIndicesQuad();
		dxVertices+=4;

		s+=PreviousPSXDisplay.Range.x1-2;

		dxVertices[0].x = s;
		dxVertices[0].y = 0;
		dxVertices[0].z = 0.00004f;
		dxVertices[0].color = color;

		dxVertices[1].x = s;
		dxVertices[1].y = PSXDisplay.DisplayMode.y;
		dxVertices[1].z = 0.00004f;
		dxVertices[1].color = color;

		dxVertices[2].x = PSXDisplay.DisplayMode.x;
		dxVertices[2].y = PSXDisplay.DisplayMode.y;
		dxVertices[2].z = 0.00004f;
		dxVertices[2].color = color;

		dxVertices[3].x = PSXDisplay.DisplayMode.x;
		dxVertices[3].y = 0;
		dxVertices[3].z = 0.00004f;
		dxVertices[3].color = color;

		addIndicesQuad();
		
		dxVertices+=4;
	}

	if(PreviousPSXDisplay.Range.y0)
	{
		s=PreviousPSXDisplay.Range.y0+1;

		dxVertices[0].x = 0;
		dxVertices[0].y = 0;
		dxVertices[0].z = 0.00004f;
		dxVertices[0].color = color;
		
		dxVertices[1].x = 0;
		dxVertices[1].y = s;
		dxVertices[1].z = 0.00004f;
		dxVertices[1].color = color;
		
		dxVertices[2].x = PSXDisplay.DisplayMode.x;
		dxVertices[2].y = s;
		dxVertices[2].z = 0.00004f;
		dxVertices[2].color = color;
		
		dxVertices[3].x = PSXDisplay.DisplayMode.x;
		dxVertices[3].y = 0;
		dxVertices[3].z = 0.00004f;
		dxVertices[3].color = color;

		addIndicesQuad();
		
		dxVertices+=4;
	}

	dxstates.setShader(PS_C);
	Draw(1);

	dxstates.scissorTest.set(1);
	dxstates.alphaTest.set(1);
}


////////////////////////////////////////////////////////////////////////
// Aspect ratio of ogl screen: simply adjusting ogl view port
////////////////////////////////////////////////////////////////////////

void SetAspectRatio(void)
{
	
}



////////////////////////////////////////////////////////////////////////
// vram read check ex (reading from card's back/frontbuffer if needed...
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamReadEx(int x, int y, int dx, int dy)
{
	
}

////////////////////////////////////////////////////////////////////////
// vram read check (reading from card's back/frontbuffer if needed... 
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamRead(int x, int y, int dx, int dy,BOOL bFront)
{
	
}

static D3DXMATRIXA16 matWorld;
static D3DXMATRIXA16 matView;
static D3DXMATRIXA16 matProj;

////////////////////////////////////////////////////////////////////////
void BK_SetView(int w, int h) {
#ifndef USE_SHADERS
	// Set up world matrix
    D3DXMatrixIdentity( &matWorld );
	D3DXMatrixIdentity( &matView );

	D3DXMatrixOrthoOffCenterLH ( &matProj, 0, w, h, 0, -1.0f, 100.0f );

    g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );
    g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
#else
	SetView(w, h);
#endif
}

////////////////////////////////////////////////////////////////////////


D3DCOLOR clearColor = 0;

void BK_SwapBuffer() {	
	// Maybe some stuff aren't displayed yet
	Draw(1);
	
	g_pd3dDevice->EndScene();
#if 1
	g_pd3dDevice->Present(NULL, NULL, NULL, NULL);	
#else
	IDirect3DTexture9* pCurFrontBuffer = pFrontBuffer[iFrontBufferSelected];
	g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0|D3DRESOLVE_CLEARRENDERTARGET|D3DRESOLVE_CLEARDEPTHSTENCIL, NULL, pCurFrontBuffer, NULL, 0, 0, NULL, 0.0f, 0, NULL );
	g_pd3dDevice->SynchronizeToPresentationInterval();
	g_pd3dDevice->Swap( pCurFrontBuffer, NULL );

	iFrontBufferSelected++;
	iFrontBufferSelected = iFrontBufferSelected % MAX_FRONT_BUFFERS;
#endif

#ifdef USE_BUFFER
	{
#ifndef _XBOX
		CUSTOMVERTEX * vb_ptr = NULL;
		unsigned int * ib_ptr = NULL;
		g_pd3dDevice->SetIndices(NULL);
		g_pd3dDevice->SetStreamSource(0, NULL, 0, 0);

		ib->Lock(0, 0, (void**)&ib_ptr, NULL);
		vb->Lock(0, 0, (void**)&vb_ptr, NULL);
		
		dxVerticesVb = vb_ptr;
		indicesIb = ib_ptr;
		
		vb->Unlock();
		ib->Unlock();
#else
		// Faster
		g_pd3dDevice->InvalidateGpuCache(dxVerticesVb, verticesCount * sizeof(CUSTOMVERTEX), 0); 
		g_pd3dDevice->InvalidateGpuCache(indicesIb, indicesCount * sizeof(int), 0);
#endif
	}
#endif

	verticesCount = 0;
	indicesCount = 0;
	prevVerticesCount = 0;	
	prevIndicesCount = 0;

	dxVertices = dxVerticesVb;
	indices = indicesIb;	

	g_pd3dDevice->BeginScene();	

	dxstates.Apply();

#ifdef USE_BUFFER
	g_pd3dDevice->SetStreamSource(0, vb, 0, sizeof(CUSTOMVERTEX));
	g_pd3dDevice->SetIndices(ib);
#endif

	g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(0xff, 0, 0, 0), 1.0f, 0 );

	//g_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
#ifndef _XBOX
	g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
#endif
	
}

////////////////////////////////////////////////////////////////////////
void BK_Clear(uint32_t flags, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	//Draw(1);
	//g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	clearColor = D3DCOLOR_ARGB( a, r, g, b );
	//g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0 );
}


void BK_ClearDepth() {
	//Draw(1);
	g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	//g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0 );
}

////////////////////////////////////////////////////////////////////////
void BK_OpaqueOn() {
	dxstates.alphaFunc.set(D3DCMP_EQUAL);
	dxstates.alphaRef.set(0);
	dxstates.blend.set(0);
}

void BK_OpaqueOff() {
	dxstates.alphaFunc.set(D3DCMP_GREATER);
	dxstates.alphaRef.set(126);
	dxstates.blend.set(1);
}

////////////////////////////////////////////////////////////////////////
void BK_ScissorEnable() {
	//dxstates.scissorTest.enable();
}
void BK_ScissorDisable()  {	
	//dxstates.scissorTest.disable();
}

void BK_Scissor(int l, int t, int r, int b)  {
	RECT scissor;
	scissor.left = l;
	scissor.right = r;
	scissor.bottom = b;
	scissor.top = t;
	//g_pd3dDevice->SetScissorRect(&scissor);
	dxstates.setScissorRect(&scissor);
}

////////////////////////////////////////////////////////////////////////
void BK_AlphaEnable() {
	dxstates.alphaTest.set(true);
}

void BK_AlphaDisable() { 
	dxstates.alphaTest.set(false);
}

////////////////////////////////////////////////////////////////////////

void BK_DepthEnable() {	
	dxstates.depthTest.set(true);
}
void BK_DepthMask() {
	//dxstates.depthFunc.set(D3DCMP_LESS);
	dxstates.depthFunc.set(D3DCMP_GREATER);
	//dxstates.stencilFunc.set(D3DCMP_ALWAYS);
	//dxstates.stencilFail.set(D3DSTENCILOP_INCRSAT);
	//dxstates.stencilPass.set(D3DSTENCILOP_INCRSAT);
}
void BK_DepthUnmask() {
	dxstates.depthFunc.set(D3DCMP_ALWAYS);
	//dxstates.stencilFunc.set(D3DCMP_LESS);
	//dxstates.stencilFail.set(D3DSTENCILOP_KEEP);
	//dxstates.stencilPass.set(D3DSTENCILOP_KEEP);
}
void BK_DepthDisable() {   
	dxstates.depthTest.set(false);
}


static bk_texture_t backend_fb;


bk_texture_t * BK_GetFrameBuffer() {
	return 	&backend_fb;
}

////////////////////////////////////////////////////////////////////////                                          
// Transparent blending settings
////////////////////////////////////////////////////////////////////////

typedef struct SEMITRANSTAG
{
	int srcFac;
	int dstFac;
	int	func;
	unsigned char alpha;
} SemiTransParams;

static SemiTransParams TransSets[4]=
{
	{D3DBLEND_SRCALPHA,		D3DBLEND_SRCALPHA,		D3DBLENDOP_ADD,	127},
	{D3DBLEND_ONE,			D3DBLEND_ONE,			D3DBLENDOP_ADD,	255},
	{D3DBLEND_ONE,			D3DBLEND_ONE,			D3DBLENDOP_REVSUBTRACT,	255},
	{D3DBLEND_INVSRCALPHA,	D3DBLEND_ONE,			D3DBLENDOP_ADD,	192}
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
	if(!DrawSemiTrans) {
		dxstates.blend.set(false);
		ubGloAlpha=ubGloColAlpha=255;                       // -> full alpha
		return;                                             // -> and bye
	}

	ubGloAlpha=ubGloColAlpha=TransSets[GlobalTextABR].alpha;

	dxstates.blend.set(true);
	dxstates.blendFunc.set(TransSets[GlobalTextABR].srcFac, TransSets[GlobalTextABR].dstFac);
	dxstates.blendOp.set(TransSets[GlobalTextABR].func);
}


////////////////////////////////////////////////////////////////////////

void BK_SetColor(unsigned char * c) {
#ifndef _XBOX
	vertex[0].c.lcol = 
		vertex[1].c.lcol = 
		vertex[2].c.lcol = 
		vertex[3].c.lcol = 
		D3DCOLOR_ARGB( c[3], c[2], c[1], c[0] );
#else
	vertex[0].c.lcol = 
		vertex[1].c.lcol = 
		vertex[2].c.lcol = 
		vertex[3].c.lcol = 
		D3DCOLOR_ARGB( c[0], c[1], c[2], c[3] );
#endif
}

////////////////////////////////////////////////////////////////////////
#define DEFAULT_TEXTURE_SIZE 1024 * 1024 * 4

/** 
* Create a big picture ! 
* and use it for all few texture caches 
**/
static LPDIRECT3DTEXTURE9 MainAtlas = NULL;

static void copyImage(int xoffset, int yoffset, int width, int height, BYTE * srcdata, BYTE * surfbuf, int pitch) 
{
	BYTE * dstdata = NULL;
	int offset = 0;

	for (int y = yoffset; y < (yoffset + height); y++) {
		offset = (y * pitch)+(xoffset * 4);
		dstdata = surfbuf + offset;

		for (int x = xoffset; x < (xoffset + width); x++) {
#ifdef _XBOX
			dstdata[0] = srcdata[0];
			dstdata[1] = srcdata[1];
			dstdata[2] = srcdata[2];
			dstdata[3] = srcdata[3];
#else
			dstdata[2] = srcdata[0];
			dstdata[1] = srcdata[1];
			dstdata[0] = srcdata[2];
			dstdata[3] = srcdata[3];
#endif
			srcdata += 4;
			dstdata += 4;
		}
	}
}

#define ATLAS_SIZE		4096
#define ATLAS_TEX_SIZE	256
#define ATLAS_NB_TEX	(ATLAS_SIZE/ATLAS_TEX_SIZE)

struct BK_Texture{
	LPDIRECT3DTEXTURE9 tex;
	void * data;
	int pitch;
	int used;
	int use_atlas;
	int iU;
	int iV;
	int textureWidth;
	int textureHeight;
};

static BK_Texture AtlasTextures[ATLAS_NB_TEX] ;


void BK_InitTextures() {
}

bk_texture_t * BK_CreateTexture(int width, int height, int use_atlas) {
	bk_texture_t * texture = new bk_texture_t;

	if (use_atlas) {
		BK_Texture * fake_texture = &AtlasTextures[0];

		// Look for an empty space
		while(fake_texture && fake_texture->used) {
			fake_texture++;
		}
		if (fake_texture == 0) {
			// Ouch !
			DebugBreak();
		}
		fake_texture->used = 1;

		texture->_private = fake_texture;
	} else {
		// Used by direct upload
		D3DLOCKED_RECT Rect;
		texture->_private = new BK_Texture;
		texture->_private->tex = NULL;
		texture->_private->use_atlas = 0;
		texture->_private->iU = 0;
		texture->_private->iV = 0;
		texture->_private->textureWidth = width;
		texture->_private->textureHeight = height;
		texture->wrap = WRAP_CLAMP;
	#ifndef _XBOX	
		g_pd3dDevice->CreateTexture(width, height, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture->_private->tex, NULL);
	#else
		g_pd3dDevice->CreateTexture(width, height, 0, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &texture->_private->tex, NULL);
	#endif
		texture->_private->tex->LockRect(0, &Rect, NULL, 0);
		texture->_private->tex->UnlockRect(0);
		texture->_private->data = Rect.pBits;
		texture->_private->pitch = Rect.Pitch;
	}
	

	return texture;
}

void BK_SetTexture(bk_texture_t * texture) {
	if (texture == NULL) {
		g_pd3dDevice->SetTexture(0, NULL);
		Draw(1);
		//dxstates.setTexture(NULL);
	} else {
		g_pd3dDevice->SetTexture(0, texture->_private->tex);

		//dxstates.setTexture(texture->_private->tex);
		//iAtlas = texture->_private->iUv;
		iAtlasU = texture->_private->iU;
		iAtlasV = texture->_private->iV;

		if (texture->_private->use_atlas == 0) {
			dxstates.SetTextureSize(
				texture->_private->textureWidth,texture->_private->textureHeight,  
				texture->_private->textureWidth,texture->_private->textureHeight);

		} else {
			dxstates.SetTextureSize(
				ATLAS_TEX_SIZE, ATLAS_TEX_SIZE,  
				ATLAS_SIZE, ATLAS_SIZE);
		}

		
		Draw(1);
	}
}

void BK_DeleteTexture(bk_texture_t * texture) {
	texture->_private->used = 0;
}

void * BK_TextureLock(bk_texture_t * texture, int * pitch) {
#ifdef _XBOX
	*pitch = texture->_private->pitch;
	return texture->_private->data;
#else
	g_pd3dDevice->SetTexture(0, NULL);
	D3DLOCKED_RECT lr;
	LPDIRECT3DTEXTURE9 tex = texture->_private->tex;

	if (texture->_private->use_atlas) {
		int rowx = texture->_private->iU;
		int rowy = texture->_private->iV;
		RECT rect;
		
		rect.top = rowy * ATLAS_TEX_SIZE;
		rect.left = rowx * ATLAS_TEX_SIZE;
		rect.bottom = rect.top + ATLAS_TEX_SIZE;
		rect.right = rect.left + ATLAS_TEX_SIZE;

		tex->LockRect(0, &lr, &rect, 0);
	} else {
		tex->LockRect(0, &lr, NULL, 0);
	}
	*pitch = lr.Pitch;
	return lr.pBits;
#endif
}
void BK_TextureUnlock(bk_texture_t * texture) {
#ifndef _XBOX
	LPDIRECT3DTEXTURE9 tex = texture->_private->tex;
	tex->UnlockRect(0);
#else
	g_pd3dDevice->InvalidateGpuCache(texture->_private->data, texture->_private->pitch * texture->_private->textureHeight, 0);
#endif
	return;
}

int BK_SetTextureData(bk_texture_t * texture, int texw, int texh, void * data) {
	if (data) {
#ifdef _XBOX
		g_pd3dDevice->SetTexture(0, NULL);
#endif

		D3DLOCKED_RECT lr;
		lr.pBits = BK_TextureLock(texture, &lr.Pitch);
   
		copyImage(0, 0, texw, texh, (BYTE*)data, (BYTE*) lr.pBits, lr.Pitch);

		BK_TextureUnlock(texture);
	}
	return 0;
}

int BK_SetSubTextureData(bk_texture_t * texture, int xoffset, int yoffset, int texw, int texh, void * data) {
	if (data) {
#ifdef _XBOX
		g_pd3dDevice->SetTexture(0, NULL);
#endif
   
		D3DLOCKED_RECT lr;
		lr.pBits = BK_TextureLock(texture, &lr.Pitch);

		//copyImage(xoffset, yoffset, texw, texh, (BYTE*)data, (BYTE*) lr.pBits, lr.Pitch);

		BK_TextureUnlock(texture);
	}
	return 0;
}

unsigned int BK_GetGlTexture(bk_texture_t * texture) {
	//return texture->_private->tex;
	return 0;
}


void BK_TextureEnable() {
	textured = 1;
}

void BK_TextureDisable() {	
	textured = 0;
}

void BK_Init() {

#ifndef _XBOX
	MoveWindow(hWWindow,                                // -> center wnd
		GetSystemMetrics(SM_CXFULLSCREEN)/2-iResX/2,
		GetSystemMetrics(SM_CYFULLSCREEN)/2-iResY/2,
		iResX+GetSystemMetrics(SM_CXFIXEDFRAME)+3,
		iResY+GetSystemMetrics(SM_CYFIXEDFRAME)+GetSystemMetrics(SM_CYCAPTION)+3,
		TRUE);
#endif
	// Create the D3D object, which is needed to create the D3DDevice.
	g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );

    // Set up the structure used to create the D3DDevice. Most parameters are
    // zeroed out. We set Windowed to TRUE, since we want to do D3D in a
    // window, and then set the SwapEffect to "discard", which is the most
    // efficient method of presenting the back buffer to the display.  And 
    // we request a back buffer format that matches the current desktop display 
    // format.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;	
	d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

#ifndef _XBOX
    g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWWindow,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice);
#else
	XVIDEO_MODE VideoMode;
	ZeroMemory( &VideoMode, sizeof( VideoMode ) );
	XGetVideoMode( &VideoMode );
	d3dpp.BackBufferWidth = 1280;
	d3dpp.BackBufferHeight = 720;
	d3dpp.Windowed = FALSE;
	
   // d3dpp.DisableAutoFrontBuffer = TRUE;
	hWWindow = NULL;
	/*
	d3dpp.RingBufferParameters.PrimarySize = 2 * 1024 * 1024;
	d3dpp.RingBufferParameters.SegmentCount = 256;
	*/
	// Release the device created by gui
#if 1
	if(g_pd3dDevice) {
		g_pd3dDevice->Release();
	}
	g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice);
#else
	// g_pd3dDevice->Reset(&d3dpp);
#endif

	g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(0xff, 0, 0, 0), 1.0f, 0 );

#endif

	// Used for scissor and framebuffer access
	iResX = d3dpp.BackBufferWidth;
	iResY = d3dpp.BackBufferHeight;

#ifdef USE_SHADERS
	CompileShaders();
	SetShaders(0);
#endif

#ifdef USE_BUFFER
#ifndef _XBOX
	g_pd3dDevice->CreateVertexBuffer(MAX_VERTICES * 3 * sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &vb, NULL);
	g_pd3dDevice->CreateIndexBuffer(MAX_VERTICES * 3 * sizeof(int), D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib, NULL);

	{
		CUSTOMVERTEX * vb_ptr = NULL;
		unsigned int * ib_ptr = NULL;
		ib->Lock(0, 0, (void**)&ib_ptr, NULL);
		vb->Lock(0, 0, (void**)&vb_ptr, NULL);
		dxVerticesVb = vb_ptr;
		indicesIb = ib_ptr;

		vb->Unlock();
		ib->Unlock();
	}
#else
	g_pd3dDevice->CreateVertexBuffer(MAX_VERTICES * 3 * sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &vb, NULL);
	g_pd3dDevice->CreateIndexBuffer(MAX_VERTICES * 3 * sizeof(int), D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ib, NULL);

	{
		CUSTOMVERTEX * vb_ptr = NULL;
		unsigned int * ib_ptr = NULL;
		ib->Lock(0, 0, (void**)&ib_ptr, D3DLOCK_NO_GPU_CACHE_INVALIDATE );
		vb->Lock(0, 0, (void**)&vb_ptr, D3DLOCK_NO_GPU_CACHE_INVALIDATE );
		dxVerticesVb = vb_ptr;
		indicesIb = ib_ptr;

		vb->Unlock();
		ib->Unlock();
	}
#endif
#endif
	dxVertices = dxVerticesVb;
	indices = indicesIb;
	
	dxVertices[0].color = 0xFFFFFFFF;
	dxVertices[1].color = 0xFFFFFFFF;
	dxVertices[2].color = 0xFFFFFFFF;
	dxVertices[3].color = 0xFFFFFFFF;

	g_pd3dDevice->BeginScene();

	g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, true);

#ifdef USE_BUFFER
	g_pd3dDevice->SetStreamSource(0, vb, 0, sizeof(CUSTOMVERTEX));
	g_pd3dDevice->SetIndices(ib);
#endif

	backend_fb._private = new BK_Texture();
//	backend_fb._private->tex = pRenderTexture[0];

#ifdef _XBOX
	g_pd3dDevice->CreateTexture(ATLAS_SIZE, ATLAS_SIZE, 0, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &MainAtlas, NULL);
#else
	g_pd3dDevice->CreateTexture(ATLAS_SIZE, ATLAS_SIZE, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &MainAtlas, NULL);
#endif
	memset(&AtlasTextures, 0, ATLAS_NB_TEX * sizeof(BK_Texture));

	uint8_t * texturePtr = NULL;

	D3DLOCKED_RECT Rect;
	MainAtlas->LockRect(0, &Rect, NULL, 0);
	MainAtlas->UnlockRect(0);

	texturePtr = (uint8_t*)Rect.pBits;

	for(int i = 0; i < ATLAS_NB_TEX; i++) {
		int iU = i % ATLAS_NB_TEX;
		int iV = i / ATLAS_NB_TEX;
		// used to get final uv
		AtlasTextures[i].use_atlas = 1;
		AtlasTextures[i].iU = iU;
		AtlasTextures[i].iV = iV;
		AtlasTextures[i].tex = MainAtlas;
		AtlasTextures[i].pitch = Rect.Pitch;
		AtlasTextures[i].data = texturePtr + (((iU * ATLAS_TEX_SIZE) + (iV * ATLAS_TEX_SIZE)) * 4); // 32 bit texture
	}

	dxstates.Initialize();
}

void BK_Exit() {
	if( g_pd3dDevice != NULL )
        g_pd3dDevice->Release();

    if( g_pD3D != NULL )
        g_pD3D->Release();
}

bk_texture_t * gTexName = NULL;
bk_texture_t * gTexFrameName = NULL;

#endif