#include "gui.h"
#include <sys/types.h>
#include "psxcommon.h"
#include "cdriso.h"
#include "r3000a.h"
#include "gpu.h"

void RenderXui();

DWORD * InGameThread() {
	while(1) {
		XINPUT_STATE InputState;
		DWORD XInputErr=XInputGetState( 0, &InputState );
		if(	XInputErr != ERROR_SUCCESS)
			continue;

		// Check if some button are pressed
		if(InputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK && InputState.Gamepad.wButtons & XINPUT_GAMEPAD_START) {
			OutputDebugStringA("Pause !!");
			PausePcsx();
			xboxConfig.ShutdownRequested = true;
		}
		Sleep(50);
	}
	return NULL;
}

#define MAX_FILENAME 256

HRESULT ShowKeyboard(std::wstring & resultText, std::wstring titleText, std::wstring descriptionText, std::wstring defaultText) {
	wchar_t result[MAX_FILENAME];
	
	XOVERLAPPED Overlapped;
	ZeroMemory( &Overlapped, sizeof( XOVERLAPPED ) );

	DWORD dwResult = XShowKeyboardUI(
		XUSER_INDEX_ANY,
		0,
		defaultText.c_str(),
		titleText.c_str(),
		descriptionText.c_str(),
		result,
		MAX_FILENAME,
		&Overlapped
	);

	while(!XHasOverlappedIoCompleted(&Overlapped)) {
		RenderXui();
	}

	resultText = result;

	return (dwResult == ERROR_SUCCESS)? S_OK : E_FAIL;
}

void SaveStatePcsx(int n) {
#if 1
	std::string game = xboxConfig.game;
	std::wstring wgame;
	std::wstring result;

	// Get basename of current game
	game.erase(0, game.rfind('\\')+1);

	get_wstring(game, wgame);

	//if (SUCCEEDED(ShowKeyboard(result, L"", L"", wgame.c_str()))) {
	ShowKeyboard(result, L"Enter the filename of the save states", L"If a file with the same name exists it will overwriten", wgame.c_str());
	{
		std::string save_path;
		std::string save_filename;
		save_path = xboxConfig.saveStateDir + "\\" + game;

		// Create save dir
		CreateDirectoryA(save_path.c_str(), NULL);

		// Save state
		get_string(result, save_filename);

		save_path += "\\" + save_filename;

		SaveState(save_path.c_str());
	}
#else
	std::string game = xboxConfig.game + ".sgz";
	SaveState(game.c_str());
#endif
}

void LoadStatePcsx(std::string save) {
	LoadState(save.c_str());

	// Resume emulation
	ResumePcsx();
}

void LoadStatePcsx(int n) {
	std::string game = xboxConfig.game + ".sgz";
	LoadState(game.c_str());

	// Resume emulation
	ResumePcsx();
}

void RenderXui() {
	g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
	0xff000000, 1.0f, 0L );

	if (xboxConfig.Running == true) {
		 DrawPcsxSurface();
	}

	app.RunFrame();
	app.Render();
	XuiTimersRun();

	// Present the backbuffer contents to the display.
	g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}


static void DoPcsx(char * game) {
	int ret;

	SetIsoFile(game);

	if (SysInit() == -1) {
		// Display an error ?
		printf("SysInit() Error!\n");
		return;
	}

	gpuDmaThreadInit();

	gpuThreadEnable(xboxConfig.UseThreadedGpu);

	ret = CDR_open();
	if (ret < 0) { SysMessage (_("Error Opening CDR Plugin")); return; }
	ret = GPU_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening GPU Plugin (%d)"), ret); return; }
	ret = SPU_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening SPU Plugin (%d)"), ret); return; }
	ret = PAD1_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening PAD1 Plugin (%d)"), ret); return; }

	CDR_init();
	GPU_init();
	SPU_init();
	PAD1_init(1);
	PAD2_init(2);
	
	GPU_clearDynarec(clearDynarec);

	SysPrintf("CheckCdrom\r\n");
	int res = CheckCdrom();
	if(res)
		SysPrintf("CheckCdrom: %08x\r\n",res);
	res=LoadCdrom();
	if(res)
		SysPrintf("LoadCdrom: %08x\r\n",res);
	
	SysReset();

	SysPrintf("Execute\r\n");
	// SysReset();
	psxCpu->Execute();
}


void ApplySettings() {
	Config.Cpu = xboxConfig.UseDynarec ? CPU_DYNAREC : CPU_INTERPRETER;
	Config.SpuIrq = xboxConfig.UseSpuIrq;

	UseFrameLimit = xboxConfig.UseFrameLimiter;
}

void LoadSettings() {
	strcpy(Config.Bios, "SCPH1001.BIN");
	strcpy(Config.BiosDir, "game:\\BIOS");
	strcpy(Config.Mcd1,"game:\\memcards\\Memcard1.mcd");
	strcpy(Config.Mcd2,"game:\\memcards\\Memcard2.mcd");

	xboxConfig.UseDynarec = 1;
	xboxConfig.UseSpuIrq = 0;
	xboxConfig.UseFrameLimiter = 1;
	xboxConfig.saveStateDir = "game:\\states";
	xboxConfig.UseThreadedGpu = 1;

	// Merge cfg file
}

static void InitPcsx() {
	// Initialize pcsx with default settings
	XMemSet(&Config, 0, sizeof(PcsxConfig));

	// Create dir ...
	CreateDirectory("game:\\memcards\\", NULL);
	CreateDirectory("game:\\patches\\", NULL);
	CreateDirectory(xboxConfig.saveStateDir.c_str(), NULL);

	Config.PsxOut = 1;
	Config.HLE = 0;
	Config.Xa = 0;  //XA enabled
	Config.Cdda = 0;
	Config.PsxAuto = 0;
	
	// Init some stuff ...
	gpuDmaThreadInit();
	cdrIsoInit();
}

static void InitXui() {
	// Initialize the xui application
	HRESULT hr = app.InitShared(g_pd3dDevice, &g_d3dpp, XuiD3DXTextureLoader, NULL);

    // Register a default typeface
    hr = app.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xui/xarialuni.ttf" );

    // Load the skin file used for the scene. 
    app.LoadSkin( L"file://game:/media/xui/simple_scene_skin.xur" );

    // Load the scene.
    app.LoadFirstScene( L"file://game:/media/xui/", L"scene.xur", NULL );

	// Start the in game thread
	HANDLE hInGameThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)InGameThread, NULL, CREATE_SUSPENDED, NULL);

	XSetThreadProcessor(hInGameThread, 5);
	ResumeThread(hInGameThread);

}

VOID __cdecl main()
{
	InitD3D();

	InitPcsx();

	LoadSettings();

	InitXui();
	/*
	// test
	xboxConfig.Running = false;
	xboxConfig.game = "game:\\patches\\ff7.iso";
	SaveStatePcsx(1);
	*/

	while(1) {
		// Render
		RenderXui();

		// a game can be loaded !
		if (xboxConfig.game.empty() == false && xboxConfig.Running == false) {
			// run it !!!			
			xboxConfig.Running = true;

			ApplySettings();

			DoPcsx((char*)xboxConfig.game.c_str());
		}
	}

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}

