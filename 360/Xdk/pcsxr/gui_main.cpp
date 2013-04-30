#include "gui.h"
#include <sys/types.h>
#include "psxcommon.h"
#include "cdriso.h"
#include "r3000a.h"

extern "C" void gpuDmaThreadInit();

void RenderXui();

//----------------------------------------------------------------------------------
// Name: RunFrame
// Desc: Overrides CXuiModule::RunFrame() to additionally poll the event handlers.
//----------------------------------------------------------------------------------
void CMyApp::RunFrame()
{
    m_waitlist.ProcessWaitHandles();
    CXuiModule::RunFrame();
}


//----------------------------------------------------------------------------------
// Name: RegisterWaitHandle
// Desc: Registers a handle for async operations.
//       When the specified handle is signaled, the application will send the 
//       specified message to hObj.  If bRemoveAfterSignaled, the handle is 
//       unregistered before the message is sent.  If the caller needs the handle 
//       to persist, then it must be registered again.
//----------------------------------------------------------------------------------
BOOL CXuiWaitList::RegisterWaitHandle( HANDLE hWait, HXUIOBJ hObj, DWORD dwMessageId, BOOL bRemoveAfterSignaled )
{
    ASSERT( m_nNumWaitHandles < MAXIMUM_WAIT_OBJECTS );
    if( m_nNumWaitHandles >= MAXIMUM_WAIT_OBJECTS )
        return FALSE;

    // Append to the wait handle array.
    m_WaitHandles[ m_nNumWaitHandles ] = hWait;

    memset( &m_WaitEntries[ m_nNumWaitHandles ], 0x00, sizeof( m_WaitEntries[ 0 ] ) );
    m_WaitEntries[ m_nNumWaitHandles ].hObj = hObj;
    m_WaitEntries[ m_nNumWaitHandles ].dwMessageId = dwMessageId;
    if( bRemoveAfterSignaled )
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NONE;
    }
    else
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NOREMOVE;
    }

    ++m_nNumWaitHandles;
    return TRUE;
}


//----------------------------------------------------------------------------------
// Name: RegisterWaitHandleFunc
// Desc: Registers a callback for when the given event fires.
//----------------------------------------------------------------------------------
BOOL CXuiWaitList::RegisterWaitHandleFunc( HANDLE hWait, PFN_WAITCOMPLETION pfnCompletion, void* pvContext,
                                           BOOL bRemoveAfterSignaled )
{
    ASSERT( m_nNumWaitHandles < MAXIMUM_WAIT_OBJECTS );
    if( m_nNumWaitHandles >= MAXIMUM_WAIT_OBJECTS )
        return FALSE;

    // Append to the wait handle array.
    m_WaitHandles[ m_nNumWaitHandles ] = hWait;

    memset( &m_WaitEntries[ m_nNumWaitHandles ], 0x00, sizeof( m_WaitEntries[ 0 ] ) );
    m_WaitEntries[ m_nNumWaitHandles ].pfnCompletionRoutine = pfnCompletion;
    m_WaitEntries[ m_nNumWaitHandles ].pvContext = pvContext;
    if( bRemoveAfterSignaled )
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NONE;
    }
    else
    {
        m_WaitEntries[ m_nNumWaitHandles ].dwFlags = WAIT_HANDLE_F_NOREMOVE;
    }

    ++m_nNumWaitHandles;
    return TRUE;
}


//----------------------------------------------------------------------------------
// Name: ProcessWaitHandles
// Desc: Checks the list of registered wait handles.  If a handle is signaled, the 
//       message that was passed to RegisterWaitHandle is sent to the associated 
//       object.
//----------------------------------------------------------------------------------
void CXuiWaitList::ProcessWaitHandles()
{
    if( m_nNumWaitHandles < 1 )
        return;

    DWORD dwRet = WaitForMultipleObjects( m_nNumWaitHandles, m_WaitHandles, FALSE, 0 );
    if( dwRet >= WAIT_OBJECT_0 && dwRet <= WAIT_OBJECT_0 + ( DWORD )m_nNumWaitHandles )
    {
        DispatchWaitEntry( dwRet - WAIT_OBJECT_0 );
    }
    else if( dwRet >= WAIT_ABANDONED_0 && dwRet <= WAIT_ABANDONED_0 + ( DWORD )m_nNumWaitHandles )
    {
        DispatchWaitEntry( dwRet - WAIT_ABANDONED_0 );
    }
    else
    {
        ASSERT( dwRet == WAIT_TIMEOUT );
    }
}


//----------------------------------------------------------------------------------
// Name: DispatchWaitEntry
// Desc: Dispatches the registered entry at the specified index.
//       This will send the message and remove the handle from the list.
//----------------------------------------------------------------------------------
void CXuiWaitList::DispatchWaitEntry( int nIndex )
{
    ASSERT( nIndex >= 0 && nIndex < m_nNumWaitHandles );
    if( nIndex < 0 || nIndex >= m_nNumWaitHandles )
        return;

    HXUIOBJ hObj = m_WaitEntries[ nIndex ].hObj;
    DWORD dwMessage = m_WaitEntries[ nIndex ].dwMessageId;

    // Now remove the entry from the wait arrays.
    if( ( m_WaitEntries[ nIndex ].dwFlags & WAIT_HANDLE_F_NOREMOVE ) == 0 )
        RemoveWaitEntry( nIndex );

    if( m_WaitEntries[ nIndex ].hObj != NULL )
    {
        // Now send the message.
        XUIMessage msg;
        XuiMessage( &msg, dwMessage );
        XuiSendMessage( hObj, &msg );
    }
    else
    {
        m_WaitEntries[ nIndex ].pfnCompletionRoutine( m_WaitEntries[ nIndex ].pvContext );
    }
}


//----------------------------------------------------------------------------------
// Name: RemoveWaitEntry
// Desc: Removes the specified index from the wait list.
//----------------------------------------------------------------------------------
void CXuiWaitList::RemoveWaitEntry( int nIndex )
{
    if( nIndex < m_nNumWaitHandles - 1 )
    {
        memmove( &m_WaitHandles[ nIndex ], &m_WaitHandles[ nIndex + 1 ], sizeof( HANDLE ) *
                 ( m_nNumWaitHandles - nIndex - 1 ) );
        memmove( &m_WaitEntries[ nIndex ], &m_WaitEntries[ nIndex + 1 ], sizeof( WaitEntry ) *
                 ( m_nNumWaitHandles - nIndex - 1 ) );
    }
    --m_nNumWaitHandles;
}


//----------------------------------------------------------------------------------
// Name: UnregisterWaitHandle
// Desc: Removes the specified handle from the wait list.
//----------------------------------------------------------------------------------
void CXuiWaitList::UnregisterWaitHandle( HANDLE hWait )
{
    int nEntryIndex = -1;
    for( int i = 0; i < m_nNumWaitHandles; ++i )
    {
        if( m_WaitHandles[ i ] == hWait )
        {
            nEntryIndex = i;
            break;
        }
    }
    ASSERT( nEntryIndex >= 0 );
    if( nEntryIndex < 0 )
        return;

    RemoveWaitEntry( nEntryIndex );
}

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

void SaveStatePcsx(int n) {
#if 0
	std::string game = xboxConfig.game;
	std::wstring wgame;
	wchar_t result[30];
	XOVERLAPPED xOl;
	DWORD KeyBoard;
	int ooo;

	ZeroMemory( &xOl, sizeof( XOVERLAPPED ) );


	game.erase(0, game.rfind('\\')+1);
	game.append(".sgz");

	get_wstring(game, wgame);

	// Show keyboard
	//KeyBoard = XShowKeyboardUI(0, VKBD_DEFAULT, wgame.c_str(), NULL, NULL, result, 30 * sizeof(wchar_t), &xOl);

	KeyBoard = XShowMessageBoxUI( 0,
            L"Title",                   // Message box title
            L"Your message goes here",  // Message string
            0,// Number of buttons
            0,             // Button captions
            0,                          // Button that gets focus
            XMB_ERRORICON,              // Icon to display
            &ooo,                  // Button pressed result
            &xOl );

	// Wait ...
	while(!XHasOverlappedIoCompleted(&xOl)) {
		RenderXui();
	}

	if (xOl.dwExtendedError == ERROR_SUCCESS) {

		game.insert(0, xboxConfig.saveStateDir);

		SaveState(game.c_str());
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

