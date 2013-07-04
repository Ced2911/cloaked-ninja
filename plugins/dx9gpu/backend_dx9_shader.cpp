#define USE_DX9
#ifdef USE_DX9
#ifdef _XBOX
#include <xtl.h>
#endif
#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9.h>

#include "stdafx.h"
#include "externals.h"

#include "backend.h"
#include "backend_dx9_shader.h"

#include <string>

const char * vscode =
    " float4x4 matWVP : register(c0);              "
	" float4 textureSize: register(c4);			   "
	" float4 outputSize: register(c5);			   "
    "                                              "
    " struct VS_IN                                 "
    "                                              "
    " {                                            "
    "		float4 ObjPos   : POSITION;              "  // Object space position 
    "		float4 Color    : COLOR0;                 "  // Vertex color                 
	"		float4 Uv    : TEXCOORD0;                 "  // Vertex color
    " };                                           "
    "                                              "
    " struct VS_OUT                                "
    " {                                            "
    "		float4 ProjPos  : POSITION;              "  // Projected space position 
    "		float4 Color    : COLOR;                 "
	"		float2 Uv    : TEXCOORD0;                 "  // Vertex color
    " };                                           "
    "                                              "
    " VS_OUT main( VS_IN In )                      "
    " {                                            "
    "		VS_OUT Out;                              "
    "		Out.ProjPos = mul( matWVP, In.ObjPos );  "  // Transform vertex into
    "		Out.Color = In.Color;                    "  // Projected space and 
	//"		Out.Uv = In.Uv+float2(0.5/720, 0.5/1280);" // Half pixel fix for directx9 pc
	//"		Out.Uv = In.Uv;" // Not needed for other
	// Atlas stuff
#if 0
	"		float2 off = float2(0, 0);							"
	//"		off.x = In.Uv.z % (4096.f/256.f);			"
	//"		off.y = In.Uv.z / (4096.f/256.f);			"
	//"		off.x = (In.Uv.z * textureSize.x) / textureSize.z;"
	//"		off.y = (In.Uv.w * textureSize.y) / textureSize.w;"
	"		Out.Uv = (In.Uv.xy / (textureSize.z/textureSize.x)) + off;			"
#elif 0
	"		Out.Uv = In.Uv.xy;" // Not needed for other
#else	
	"		float2 off = float2(0, 0);							"
	"		off.x = (In.Uv.z * textureSize.x) / textureSize.z;"
	"		off.y = (In.Uv.w * textureSize.y) / textureSize.w;"
	"		Out.Uv = ((In.Uv.xy+float2(0.5/720, 0.5/1280)) / (textureSize.z/textureSize.x)) + off;			"
#endif
    "		return Out;                              "  // Transfer color
    " }                                            ";



//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const char * Gpscode =
	" sampler s: register(s0);					   "
	" float use_texture: register(c0);			   "
    " struct PS_IN                                 "
    " {                                            "
	"     float4 Color : COLOR;                    "  
    "     float2 Uv : TEXCOORD0;                   "                     
    " };                                           " 
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     float4 c =  tex2D(s, In.Uv)  ;           "  // Output color
    "	  c *= In.Color;"
    "	  c.rgb *= 2.0f;"
	"     return c;"
    " }                                            ";

const char * Fpscode =
	" sampler s: register(s0);					   "
	" float use_texture: register(c0);			   "
    " struct PS_IN                                 "
    " {                                            "
	"     float4 Color : COLOR;                    "  
    "     float2 Uv : TEXCOORD0;                   "                     
    " };                                           " 
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     float4 c =  tex2D(s, In.Uv)  ;           "  // Output color
	//"		c.rgb *= 2.0f;"
	"		return c;"
    " }                                            ";

const char * Cpscode =
	" sampler s: register(s0);					   "
	" float use_texture: register(c0);			   "
    " struct PS_IN                                 "
    " {                                            "
    "     float4 Color : COLOR;                    "  // Interpolated color from  
	"     float2 Uv : TEXCOORD0;                   "          
    " };                                           "  // the vertex shader
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     return In.Color;                         "  // Output color
    " }                                            ";

static IDirect3DVertexDeclaration9* pVertexDecl = NULL;

static const D3DVERTEXELEMENT9  VertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{ 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};



LPDIRECT3DVERTEXSHADER9      pVertexShader; // Vertex Shader
LPDIRECT3DPIXELSHADER9       pPixelGShader;  // Pixel Shader
LPDIRECT3DPIXELSHADER9       pPixelFShader;  // Pixel Shader
LPDIRECT3DPIXELSHADER9       pPixelCShader;  // Pixel Shader

void CompileShaders() {
	ID3DXBuffer* pShaderCode;
	ID3DXBuffer* pErrorMsg;
	HRESULT hr;
	// Compile vertex shader.
	hr = D3DXCompileShader( vscode, 
		(UINT)strlen( vscode ),
		NULL, 
		NULL, 
		"main", 
		"vs_2_0", 
		0, 
		&pShaderCode, 
		&pErrorMsg,
		NULL );
	if( FAILED(hr) )
	{
		std::string error = (CHAR*)pErrorMsg->GetBufferPointer();
		error += "\n\n";
		error += vscode;
#ifdef _XBOX
		OutputDebugStringA(error.c_str());
#else
		MessageBoxA(0, error.c_str(), "Error compiling vertex shader", MB_ICONERROR);
#endif
	}

	// Create pixel shader.
	g_pd3dDevice->CreateVertexShader( (DWORD*)pShaderCode->GetBufferPointer(), 
		&pVertexShader );


	// Compile pixel shader.
	hr = D3DXCompileShader( Gpscode, 
		(UINT)strlen( Gpscode ),
		NULL, 
		NULL, 
		"main", 
		"ps_2_0", 
		0, 
		&pShaderCode, 
		&pErrorMsg,
		NULL );
	if( FAILED(hr) )
	{
		std::string error = (CHAR*)pErrorMsg->GetBufferPointer();
		error += "\n\n";
		error += Gpscode;
#ifdef _XBOX
		OutputDebugStringA(error.c_str());
#else
		MessageBoxA(0, error.c_str(), "Error compiling pixel shader", MB_ICONERROR);
#endif
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( (DWORD*)pShaderCode->GetBufferPointer(), 
		&pPixelGShader );

	// Compile pixel shader.
	hr = D3DXCompileShader( Fpscode, 
		(UINT)strlen( Fpscode ),
		NULL, 
		NULL, 
		"main", 
		"ps_2_0", 
		0, 
		&pShaderCode, 
		&pErrorMsg,
		NULL );
	if( FAILED(hr) )
	{
		std::string error = (CHAR*)pErrorMsg->GetBufferPointer();
		error += "\n\n";
		error += Fpscode;
#ifdef _XBOX
		OutputDebugStringA(error.c_str());
#else
		MessageBoxA(0, error.c_str(), "Error compiling pixel shader", MB_ICONERROR);
#endif
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( (DWORD*)pShaderCode->GetBufferPointer(), 
		&pPixelFShader );

	// Compile pixel shader.
	hr = D3DXCompileShader( Cpscode, 
		(UINT)strlen( Cpscode ),
		NULL, 
		NULL, 
		"main", 
		"ps_2_0", 
		0, 
		&pShaderCode, 
		&pErrorMsg,
		NULL );
	if( FAILED(hr) )
	{
		std::string error = (CHAR*)pErrorMsg->GetBufferPointer();
		error += "\n\n";
		error += Cpscode;
#ifdef _XBOX
		OutputDebugStringA(error.c_str());
#else
		MessageBoxA(0, error.c_str(), "Error compiling pixel shader", MB_ICONERROR);
#endif
	}

	// Create pixel shader.
	g_pd3dDevice->CreatePixelShader( (DWORD*)pShaderCode->GetBufferPointer(), 
		&pPixelCShader );

	g_pd3dDevice->CreateVertexDeclaration( VertexElements, &pVertexDecl );
	g_pd3dDevice->SetVertexDeclaration( pVertexDecl );
}

static D3DXMATRIXA16 matWorld;
static D3DXMATRIXA16 matView;
static D3DXMATRIXA16 matProj;

void SetShaders(int type)
{
	switch(type) {
		case PS_C:
		g_pd3dDevice->SetPixelShader(pPixelCShader);
		break;
		case PS_G:
		g_pd3dDevice->SetPixelShader(pPixelGShader);
		break;
	}

	g_pd3dDevice->SetVertexShader(pVertexShader);
	g_pd3dDevice->SetVertexDeclaration( pVertexDecl );
}

void SetView(int w, int h) {
	// Set up world matrix
    D3DXMatrixIdentity( &matWorld );
	D3DXMatrixIdentity( &matView );

	D3DXMatrixOrthoOffCenterLH ( &matProj, 0, w, h, 0, -1.0f, 100.0f );
	

	g_pd3dDevice->SetVertexShaderConstantF(0, matProj, 4);
	//D3DXMatrixMultiply(
}
#endif