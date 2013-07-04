#pragma once

#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9.h>

#include "stdafx.h"
#include "externals.h"
#include "backend.h"

extern LPDIRECT3DDEVICE9   g_pd3dDevice;

void CompileShaders();
void SetShaders(int textured);

void SetView(int w, int h);

enum SHADER_TYPE{
	PS_C, // C
	PS_G, // Tex + C
};