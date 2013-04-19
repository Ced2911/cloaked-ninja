#include <xtl.h>
#include <xboxmath.h>
#include "xb_video.h"

//--------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//--------------------------------------------------------------------------------------

static const char* g_strVertexShaderProgram =
	"float4x4 matWVP : register(c0);							\n"
	"struct VS_IN												\n"
	"{															\n"
	"   float4   Position  : POSITION;							\n"
	"   float2   UV        : TEXCOORD0;							\n"
	"   float4   Color     : COLOR;								\n"  // Vertex color      
	"};															\n"
	"															\n"
	"struct VS_OUT												\n"
	"{															\n"
	"   float4 Position  : POSITION;							\n"
	"   float2 UV        : TEXCOORD0;							\n"
	"   float4 Color     : COLOR;								\n"  // Vertex color   
	"};															\n"
	"															\n"
	"VS_OUT main( VS_IN In )					\n"
	"{															\n"
	"   VS_OUT Out;												\n"
	//"	Out.Position = mul( matWVP, In.Position );				\n"
	"	Out.Position = In.Position;								\n"
	"	Out.Color = In.Color;									\n"
	//"   Out.UV  = 1.0f - (0.5f + (sign(In.Position.xy)*0.5f));										\n"
	"   Out.UV  = In.UV;										\n"
	"   return Out;												\n"
	"}															\n";


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const char*                                 g_strPixelShaderProgram =
	" struct PS_IN                                 "
	" {                                            "
	"     float2 TexCoord	: TEXCOORD;            "
	"     float4 Color		: COLOR;		       "
	" };                                           "  // the vertex shader
	"                                              "
	" sampler detail;                              "
	"                                              "
	" float4 main( PS_IN In ) : COLOR              "
	" {                                            "
	"     return tex2D( detail, In.TexCoord );     "  // Output color
	" }	                                       ";

//--------------------------------------------------------------------------------------
// Globals
//-------------------------------------------------------------------------------------
IDirect3D9*						g_pD3D = NULL; // Used to create the D3DDevice
IDirect3DDevice9*				g_pd3dDevice = NULL; // the rendering device
LPDIRECT3DTEXTURE9				g_PsxTexture = NULL; // PSX Texture
IDirect3DPixelShader9*			pPixelShader;
IDirect3DVertexShader9*			pVertexShader;
IDirect3DVertexDeclaration9*	pVertexDecl;

XMMATRIX matWVP;

extern "C" unsigned char *pPsxScreen;
extern "C" unsigned int g_pPitch;

// Structure to hold vertex data.
struct COLORVERTEX
{
	float       Position[3];
	float       TextureUV[2];
	DWORD       Color;
};

COLORVERTEX Vertices[] =
{
	// {  X,	Y,	  Z,	U,	 V,		Color	}
	{ -1.f, -1.f, 0.0f,  0.0f,  1.0f, 0xFFFF0000 }, //1
	{ -1.f,  1.f, 0.0f,  0.0f,  0.0f, 0xFF0FF000 }, //2
	{  1.f, -1.f, 0.0f,  1.0f,  1.0f, 0xFF00FF00 }, //3
};

// Define the vertex elements.
static const D3DVERTEXELEMENT9 VertexElements[4] =
{
	{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 20, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	D3DDECL_END()
};

extern "C" unsigned int  VideoInit()
{
	// Create the D3D object.
	g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );

	// Set up the structure used to create the D3DDevice.
	XVIDEO_MODE VideoMode;
	ZeroMemory( &VideoMode, sizeof( VideoMode ) );
	XGetVideoMode( &VideoMode );
	BOOL bEnable720p = ( VideoMode.dwDisplayHeight >= 720 ) ? TRUE : FALSE;

	// Set up the structure used to create the D3DDevice.
	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory( &d3dpp, sizeof( d3dpp ) );
	d3dpp.BackBufferWidth = bEnable720p ? 1280 : 640;
	d3dpp.BackBufferHeight = bEnable720p ? 720  : 480;
	d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
	d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
	d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
	d3dpp.MultiSampleQuality = 0;
	d3dpp.BackBufferCount = 1;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	// Create the Direct3D device.
	g_pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_BUFFER_2_FRAMES,
		&d3dpp, &g_pd3dDevice );

	// Buffers to hold compiled shaders and possible error messages
	ID3DXBuffer* pShaderCode = NULL;
	ID3DXBuffer* pErrorMsg = NULL;

	// Compile vertex shader.
	HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram, ( UINT )strlen( g_strVertexShaderProgram ),
		NULL, NULL, "main", "vs_2_0", 0,
		&pShaderCode, &pErrorMsg, NULL );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return E_FAIL;
	}

	// Create vertex shader.
	g_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pVertexShader );

	// Shader code is no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Compile pixel shader.
	hr = D3DXCompileShader( g_strPixelShaderProgram, ( UINT )strlen( g_strPixelShaderProgram ),
		NULL, NULL, "main", "ps_2_0", 0,
		&pShaderCode, &pErrorMsg, NULL );
	if( FAILED( hr ) )
	{
		OutputDebugStringA( pErrorMsg ? ( CHAR* )pErrorMsg->GetBufferPointer() : "" );
		return E_FAIL;
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
		&pPixelShader );

	// Shader code no longer required.
	pShaderCode->Release();
	pShaderCode = NULL;

	// Create a vertex declaration from the element descriptions.
	g_pd3dDevice->CreateVertexDeclaration( VertexElements, &pVertexDecl );
		
	// Set shaders.
	g_pd3dDevice->SetVertexShader( pVertexShader );
	g_pd3dDevice->SetPixelShader( pPixelShader );

	// Set the vertex declaration.
	g_pd3dDevice->SetVertexDeclaration( pVertexDecl );

	// Create texture
	D3DXCreateTexture(
			g_pd3dDevice, PSX_WIDTH,
			PSX_HEIGHT, D3DX_DEFAULT, 0, D3DFMT_LIN_X8R8G8B8, D3DPOOL_MANAGED,
			&g_PsxTexture
			);
	D3DLOCKED_RECT texture_info;
	g_PsxTexture->LockRect( 0,  &texture_info, NULL, NULL );
	pPsxScreen = (unsigned char *)texture_info.pBits;
	g_PsxTexture->UnlockRect(0);

	g_pPitch = texture_info.Pitch;

	// Clear psx surface
	//XMemSet(pPsxScreen, 0, texture_info.Pitch * PSX_HEIGHT);

	UpdateScrenRes(PSX_WIDTH, PSX_HEIGHT);
	return S_OK;
}

extern "C" void UpdateScrenRes(int x,int y){
	g_pd3dDevice->SetTexture(0, g_PsxTexture);	

	// Update Vertices
	Vertices[0].TextureUV[1] = (float) y / (float) PSX_HEIGHT;

	Vertices[2].TextureUV[0] = (float) x / (float) PSX_WIDTH;
	Vertices[2].TextureUV[1] = (float) y / (float) PSX_HEIGHT;
}

extern "C" void UnlockLockDisplay(){
	D3DLOCKED_RECT texture_info;

	g_pd3dDevice->SetTexture(0, NULL);

	g_PsxTexture->LockRect( 0,  &texture_info, NULL, NULL );
	g_PsxTexture->UnlockRect(0);
};

extern "C" void XbDispUpdate()
{
	// Clear the backbuffer.
	g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
		0xff00FF00, 1.0f, 0L );

	g_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, Vertices, sizeof( COLORVERTEX ) );

	// Present the backbuffer contents to the display.
	g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}
