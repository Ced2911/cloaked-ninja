#include "gui.h"
#include <sys/types.h>
#include "psxcommon.h"
#include "cdriso.h"
#include "r3000a.h"
#include "gpu.h"
#include "sys\Mount.h"
#include "simpleini\SimpleIni.h"
#include <xfilecache.h>

#define MAX_FILENAME 256


CSimpleIniA ini;

void RenderXui();
extern "C" void cdrThreadInit(void);
extern "C" void POKOPOM_Init();
extern void SetIso(const char * fname);

static float xui_time;
static float fTicksPerMicrosecond;


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
	// ûpdate xui time
	xui_time = (__mftb()/fTicksPerMicrosecond) - xui_time;

	g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
	0xff000000, 1.0f, 0L );

	if (xboxConfig.Running == true) {
		 DrawPcsxSurface();
	}

	app.RunFrame(xui_time);
	app.Render();
	XuiTimersRun();

	// Present the backbuffer contents to the display.
	// g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
	VideoPresent();
}


static void DoPcsx(char * game) {
	int ret;

	cdrIsoInit();
	POKOPOM_Init();
	//cdrThreadInit();
	SetIso(game);

	
	gpuDmaThreadInit();


	if (SysInit() == -1) {
		// Display an error ?
		printf("SysInit() Error!\n");
		return;
	}
	
	gpuThreadEnable(xboxConfig.UseThreadedGpu);
	
	GPU_clearDynarec(clearDynarec);
	
	ret = CDR_open();
	if (ret < 0) { SysMessage (_("Error Opening CDR Plugin")); return; }
	ret = GPU_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening GPU Plugin (%d)"), ret); return; }
	ret = SPU_open(NULL);	
	if (ret < 0) { SysMessage (_("Error Opening SPU Plugin (%d)"), ret); return; }	
	SPU_registerCallback(SPUirq);
	ret = PAD1_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening PAD1 Plugin (%d)"), ret); return; }	
	ret = PAD2_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening PAD2 Plugin (%d)"), ret); return; }	
	
	PAD1_registerVibration(GPU_visualVibration);	 
	PAD2_registerVibration(GPU_visualVibration);   

	//Config.SlowBoot = 1;

	// Execute bios

	SysPrintf("CheckCdrom\r\n");
	int res = CheckCdrom();
	if(res)
		SysPrintf("CheckCdrom: %08x\r\n",res);

	
	SysReset();

	res=LoadCdrom();
	if(res)
		SysPrintf("LoadCdrom: %08x\r\n",res);

	SysPrintf("Execute\r\n");
	
	//memset(&psxRegs, 0, sizeof(psxRegs));

	// Start in bootstrap (only work with .cue)
	//psxRegs.pc = 0xbfc00000; 
	//SysReset();

	//psxExecuteBios();
	psxCpu->Execute();
}


void ApplySettings() {
	Config.Cpu = xboxConfig.UseDynarec ? CPU_DYNAREC : CPU_INTERPRETER;
	Config.SpuIrq = xboxConfig.UseSpuIrq;

	if (xboxConfig.region == 0)
		Config.PsxAuto = 0;
	else {
		if (xboxConfig.region == 1) {
			Config.PsxType = PSX_TYPE_NTSC;
		} else if (xboxConfig.region == 2) {
			Config.PsxType = PSX_TYPE_PAL;
		}
		Config.PsxAuto = 1;
	}

	UseFrameLimit = xboxConfig.UseFrameLimiter;

	ini.SetLongValue("pcsx", "UseDynarec", xboxConfig.UseDynarec);
	ini.SetLongValue("pcsx", "UseSpuIrq", xboxConfig.UseSpuIrq);
	ini.SetLongValue("pcsx", "UseFrameLimiter", xboxConfig.UseFrameLimiter);
	ini.SetLongValue("pcsx", "UseThreadedGpu", xboxConfig.UseThreadedGpu);
	ini.SetLongValue("pcsx", "Region", xboxConfig.region);

	ini.SetValue("pcsx", "saveStateDir", xboxConfig.saveStateDir.c_str());
	ini.SetValue("pcsx", "Bios", Config.Bios);	
	ini.SetValue("pcsx", "BiosDir", Config.BiosDir);
	ini.SetValue("pcsx", "Mcd1", Config.Mcd1);
	ini.SetValue("pcsx", "Mcd2", Config.Mcd2);

	ini.SaveFile("game:\\pcsx.ini");
}

void LoadSettings() {
	/// defalut
	strcpy(Config.Bios, "SCPH1001.BIN");
	strcpy(Config.BiosDir, "game:\\BIOS");
	strcpy(Config.Mcd1,"game:\\memcards\\Memcard1.mcd");
	strcpy(Config.Mcd2,"game:\\memcards\\Memcard2.mcd");

	xboxConfig.UseDynarec = 1;
	xboxConfig.UseSpuIrq = 0;
	xboxConfig.UseFrameLimiter = 1;
	xboxConfig.saveStateDir = "game:\\states";
	xboxConfig.UseThreadedGpu = 1;
	xboxConfig.region = 0;// Auto

	// Merge cfg file
	ini.LoadFile("game:\\pcsx.ini");

	xboxConfig.UseDynarec = ini.GetLongValue("pcsx", "UseDynarec", 1);
	xboxConfig.UseSpuIrq = ini.GetLongValue("pcsx", "UseSpuIrq", 0);
	xboxConfig.UseFrameLimiter = ini.GetLongValue("pcsx", "UseFrameLimiter", 1);
	xboxConfig.UseThreadedGpu = ini.GetLongValue("pcsx", "UseThreadedGpu", 1);
	xboxConfig.region = ini.GetLongValue("pcsx", "Region", 0);

	xboxConfig.saveStateDir = strdup(ini.GetValue("pcsx", "saveStateDir", "game:\\states"));
	strcpy(Config.Bios, strdup(ini.GetValue("pcsx", "Bios", "SCPH1001.BIN")));
	strcpy(Config.BiosDir, strdup(ini.GetValue("pcsx", "BiosDir", "game:\\BIOS")));
	strcpy(Config.Mcd1, strdup(ini.GetValue("pcsx", "Mcd1", "game:\\memcards\\Memcard1.mcd")));
	strcpy(Config.Mcd2, strdup(ini.GetValue("pcsx", "Mcd2", "game:\\memcards\\Memcard2.mcd")));
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
	Config.Widescreen = 0;
	
	// Init some stuff ...
	//gpuDmaThreadInit();
	//cdrIsoInit();
}

static void InitXui() {
	// Initialize the xui application
	HRESULT hr = app.InitShared(g_pd3dDevice, &g_d3dpp);

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

	// Init xui time
	xui_time = 0;
	LARGE_INTEGER TicksPerSecond;
    QueryPerformanceFrequency( &TicksPerSecond );
    fTicksPerMicrosecond = (float)TicksPerSecond.QuadPart * 0.000001;
}

void Add(char * d, char * mountpoint){
	Map(mountpoint,d);
}

HRESULT MountDevices(){

	Add("\\Device\\Cdrom0","cdrom:");

	Add("\\Device\\Flash","flash:");

	//hdd
	Add("\\Device\\Harddisk0\\SystemPartition\\","hddx:");
	Add("\\Device\\Harddisk0\\Partition0","hdd0:");
	Add("\\Device\\Harddisk0\\Partition1","hdd1:");
	Add("\\Device\\Harddisk0\\Partition2","hdd2:");
	Add("\\Device\\Harddisk0\\Partition3","hdd3:");

	//mu
	Add("\\Device\\Mu0","mu0:");
	Add("\\Device\\Mu1","mu1:");
	Add("\\Device\\Mass0PartitionFile\\Storage","usbmu0:");

	//usb
	Add("\\Device\\Mass0", "usb0:");
	Add("\\Device\\Mass1", "usb1:");
	Add("\\Device\\Mass2", "usb2:");

	Add("\\Device\\BuiltInMuSfc","sfc:");

	return S_OK;
}

VOID __cdecl main()
{
	//if(ERROR_SUCCESS == XFileCacheInit( XFILECACHE_CLEAR_ALL, XFILECACHE_MAX_SIZE, XFILECACHE_DEFAULT_THREAD, 0, 1))
	{
		// Caching is enabled.
	}


	MountDevices();

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

