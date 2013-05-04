#include "gui.h"
#include "gui_class.h"

CMyApp app;
XboxConfig xboxConfig;


FileBrowserProvider fileList;
FileBrowserProvider saveStateList;
FileBrowserProvider effectList;


class CXuiFileBrowser : public CXuiListImpl
{
protected:
	FileBrowserProvider * fileBrowser;

public:
	// Message map. Here we tie messages to message handlers.
	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_GET_SOURCE_TEXT( OnGetSourceText )
		XUI_ON_XM_GET_ITEMCOUNT_ALL( OnGetItemCountAll )
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
		XUI_END_MSG_MAP()

		//----------------------------------------------------------------------------------
		// Performs initialization tasks - retrieves controls.
		//----------------------------------------------------------------------------------
		HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
	{
		fileBrowser->Init();
		fileBrowser->UpdateDirList();

		return S_OK;
	}

	//----------------------------------------------------------------------------------
	// Returns the number of items in the list.
	//----------------------------------------------------------------------------------
	HRESULT OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
	{
		pGetItemCountData->cItems = fileBrowser->Size();
		bHandled = TRUE;
		return S_OK;
	}

	//----------------------------------------------------------------------------------
	// Returns the text for the items in the list.
	//----------------------------------------------------------------------------------
	HRESULT OnGetSourceText( XUIMessageGetSourceText* pGetSourceTextData, BOOL& bHandled )
	{
		if( pGetSourceTextData->bItemData && pGetSourceTextData->iItem >= 0 )
		{
			pGetSourceTextData->szText = fileBrowser->At(pGetSourceTextData->iItem);
			bHandled = TRUE;
		}
		return S_OK;
	}

	virtual HRESULT OnNotifyPress( HXUIOBJ hObjPressed, 
		BOOL& bHandled )
	{
		int nIndex;

		// Change the current dir
		if (fileBrowser->IsDir(GetCurSel())) {
			// Delete old item count
			DeleteItems(0, fileBrowser->Size());

			// Update filelist
			fileBrowser->UpdateDirList(fileBrowser->Filename(GetCurSel()));

			// Insert item count
			InsertItems(0, fileBrowser->Size());

			// Move to top
			SetCurSel(0);			

			// Don't Notify parent
			bHandled = TRUE;

		} else {		
			get_string(fileBrowser->Filename(GetCurSel()), xboxConfig.game);

			// scene must done some work
			bHandled = FALSE;
		}
		return S_OK;
	}
};

//--------------------------------------------------------------------------------------
class CEffectBrowser : public CXuiFileBrowser
{
public:
	CEffectBrowser() {
		fileBrowser = &effectList;
	}

	virtual HRESULT OnNotifyPress( HXUIOBJ hObjPressed, 
		BOOL& bHandled )
	{
		int nIndex;

		// Change the current dir
		if (fileBrowser->IsDir(GetCurSel())) {
			// Delete old item count
			DeleteItems(0, fileBrowser->Size());

			// Update filelist
			fileBrowser->UpdateDirList(fileBrowser->Filename(GetCurSel()));

			// Insert item count
			InsertItems(0, fileBrowser->Size());

			// Move to top
			SetCurSel(0);			

			// Don't Notify parent
			bHandled = TRUE;

		} else {	
			std::string path;
			get_string(fileBrowser->Filename(GetCurSel()), path);

			LoadShaderFromFile(path.c_str());
			bHandled = FALSE;
		}
		return S_OK;
	}

public:

	// Define the class. The class name must match the ClassOverride property
	// set for the scene in the UI Authoring tool.
	XUI_IMPLEMENT_CLASS( CEffectBrowser, L"EffectBrowser", XUI_CLASS_LIST )
};

//--------------------------------------------------------------------------------------
class CLoadStateBrowser : public CXuiFileBrowser
{
public:
	CLoadStateBrowser() {
		fileBrowser = &saveStateList;
	}


	virtual HRESULT OnNotifyPress( HXUIOBJ hObjPressed, 
		BOOL& bHandled )
	{
		int nIndex;

		// Change the current dir
		if (fileBrowser->IsDir(GetCurSel())) {
			// Delete old item count
			DeleteItems(0, fileBrowser->Size());

			// Update filelist
			fileBrowser->UpdateDirList(fileBrowser->Filename(GetCurSel()));

			// Insert item count
			InsertItems(0, fileBrowser->Size());

			// Move to top
			SetCurSel(0);			

			// Don't Notify parent
			bHandled = TRUE;

		} else {	
			std::string path;
			get_string(fileBrowser->Filename(GetCurSel()), path);

			LoadStatePcsx(path);
			bHandled = FALSE;
		}
		return S_OK;
	}

public:

	// Define the class. The class name must match the ClassOverride property
	// set for the scene in the UI Authoring tool.
	XUI_IMPLEMENT_CLASS( CLoadStateBrowser, L"LoadStateBrowser", XUI_CLASS_LIST )
};

//--------------------------------------------------------------------------------------
class CFileBrowserList : public CXuiFileBrowser
{
public:
	CFileBrowserList() {
		fileBrowser = &fileList;
	}


	virtual HRESULT OnNotifyPress( HXUIOBJ hObjPressed, 
		BOOL& bHandled )
	{
		int nIndex;

		// Change the current dir
		if (fileBrowser->IsDir(GetCurSel())) {
			// Delete old item count
			DeleteItems(0, fileBrowser->Size());

			// Update filelist
			fileBrowser->UpdateDirList(fileBrowser->Filename(GetCurSel()));

			// Insert item count
			InsertItems(0, fileBrowser->Size());

			// Move to top
			SetCurSel(0);			

			// Don't Notify parent
			bHandled = TRUE;

		} else {		
			get_string(fileBrowser->Filename(GetCurSel()), xboxConfig.game);

			// scene must done some work
			bHandled = FALSE;
		}
		return S_OK;
	}

public:

	// Define the class. The class name must match the ClassOverride property
	// set for the scene in the UI Authoring tool.
	XUI_IMPLEMENT_CLASS( CFileBrowserList, L"FileBrowserList", XUI_CLASS_LIST )
};


class COsdMenuScene: public CXuiSceneImpl
{
	std::string game;
	std::wstring wgame;

	CXuiControl BackBtn;
	CXuiControl ResetBtn;
	CXuiControl SelectBtn;

	CXuiControl SaveStateBtn;
	CXuiControl LoadStateBtn;

	CXuiList LoadStateBrowser;
	CXuiControl LoadStateOkBtn;
	CXuiControl LoadStateCancelBtn;


	CXuiControl SaveStateFilename;
	CXuiControl SaveKeyboardBtn;
	CXuiControl SaveStateCancelBtn;
	CXuiControl SaveStateOkBtn;

	COsdMenuScene() {
		m_pszName = NULL;
	}

	~COsdMenuScene() {
		if( NULL != m_hEvent )
		{
			CloseHandle( m_hEvent );
			m_hEvent = NULL;
		}
		delete []m_pszName;
		m_pszName = NULL;
	}

	LPWSTR m_pszName;			// Used by XShowKeyboardUI().
	XOVERLAPPED m_Overlapped;	// Used by XShowKeyboardUI().
	HANDLE m_hEvent;			// Used by XShowKeyboardUI().
	BOOL m_bKeyboardActive;

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
		XUI_END_MSG_MAP()

		//----------------------------------------------------------------------------------
		// Performs initialization tasks - retrieves controls.
		//----------------------------------------------------------------------------------
		HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
	{
		GetChildById( L"BackBtn", &BackBtn );
		GetChildById( L"ResetBtn", &ResetBtn );
		GetChildById( L"SelectBtn", &SelectBtn );

		GetChildById( L"SaveStateBtn", &SaveStateBtn );
		GetChildById( L"LoadStateBtn", &LoadStateBtn );

		GetChildById( L"LoadStateBrowser", &LoadStateBrowser );
		GetChildById( L"LoadStateOkBtn", &LoadStateOkBtn );
		GetChildById( L"LoadStateCancelBtn", &LoadStateCancelBtn );

		GetChildById( L"SaveStateFilename", &SaveStateFilename );
		GetChildById( L"SaveStateCancelBtn", &SaveStateCancelBtn );
		GetChildById( L"SaveStateOkBtn", &SaveStateOkBtn );
		GetChildById( L"SaveKeyboardBtn", &SaveKeyboardBtn );


		ShowLoad(false);
		ShowSave(false);

		m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		m_bKeyboardActive = false;


		m_pszName = new wchar_t[ 256 ];

		effectList.UpdateDirList(L"game:\\hlsl");

		return S_OK;
	}

	void ShowLoad(bool visible)
	{
		LoadStateBrowser.SetShow(visible);
		LoadStateOkBtn.SetShow(visible);
		LoadStateCancelBtn.SetShow(visible);

		if (visible) {
			// path
			std::string path = xboxConfig.game;
			path.erase(0, path.rfind('\\')+1);

			std::string saveStateDir = xboxConfig.saveStateDir + "\\" + path;
			std::wstring wg;
			get_wstring(saveStateDir, wgame);
			saveStateList.UpdateDirList(wgame);

			LoadStateBrowser.SetFocus();
		} else {
			BackBtn.SetFocus();
		}
	}

	void ShowSave(bool visible)
	{
		SaveStateFilename.SetShow(visible);
		SaveStateCancelBtn.SetShow(visible);
		SaveStateOkBtn.SetShow(visible);

		if (visible) {
			SaveKeyboardBtn.SetFocus();
		} else {
			BackBtn.SetFocus();
		}
	}

	HRESULT OnNotifyPress( HXUIOBJ hObjPressed, 
		BOOL& bHandled )
	{

		if (hObjPressed == BackBtn) {
			ResumePcsx();
		}

		if (hObjPressed == ResetBtn) {
			ResetPcsx();
		}

		if (hObjPressed == SelectBtn) {
			xboxConfig.game.clear();
			ShutdownPcsx();
			xboxConfig.Running = false;
			NavigateBackToFirst();
		}

		// show
		if (hObjPressed == SaveStateBtn) {
			ShowSave(true);
			game = xboxConfig.game;
			game.erase(0, game.rfind('\\')+1);
			game.append(".sgz");

			get_wstring(game, wgame);

			SaveStateFilename.SetText(wgame.c_str());
		}

		if (hObjPressed == LoadStateBtn) {			
			ShowLoad(true);
		}

		if (hObjPressed == SaveStateOkBtn) {
			ShowSave(false);
			SaveStatePcsx(-1);
		}

		if (hObjPressed == LoadStateOkBtn) {			
			ShowLoad(false);
			//LoadStatePcsx(-1);
		}

		if (hObjPressed == LoadStateCancelBtn) {			
			ShowLoad(false);
			//LoadStatePcsx(-1);
		}

		if (hObjPressed == LoadStateBrowser) {			
			ShowLoad(false);
			//LoadStatePcsx(-1);
		}

		// cancel
		if (hObjPressed == SaveStateOkBtn) {
			ShowSave(false);
		}

		if (hObjPressed == LoadStateOkBtn) {			
			ShowLoad(false);
		}

		bHandled = TRUE;
		return S_OK;
	}
public:

	XUI_IMPLEMENT_CLASS( COsdMenuScene, L"InGameMenu", XUI_CLASS_SCENE )
};

class CMainMenuScene : public CXuiSceneImpl
{
	wchar_t fileBrowserInfoText[512];

	CXuiList FileBrowser;
	CXuiControl FileBrowserInfo;
	CXuiNavButton OsdBtn;
	CXuiCheckbox DynarecCbox;
	CXuiCheckbox GpuThCbox;
	CXuiCheckbox SpuIrqCbox;
	CXuiCheckbox FrameLimitCbox;

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
		XUI_ON_XM_RENDER( OnRender )
		XUI_END_MSG_MAP()


		//----------------------------------------------------------------------------------
		// Performs initialization tasks - retrieves controls.
		//----------------------------------------------------------------------------------
		HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
	{
		GetChildById( L"FileBrowser", &FileBrowser );
		GetChildById( L"OsdBtn", &OsdBtn );
		GetChildById( L"FileBrowserInfo", &FileBrowserInfo );

		GetChildById( L"DynarecCbox", &DynarecCbox );
		GetChildById( L"GpuThCbox", &GpuThCbox );
		GetChildById( L"SpuIrqCbox", &SpuIrqCbox );
		GetChildById( L"GpuThCbox", &GpuThCbox );		

		GetChildById( L"FrameLimitCbox", &FrameLimitCbox );

		// Init values from xbox config
		DynarecCbox.SetCheck(xboxConfig.UseDynarec);
		SpuIrqCbox.SetCheck(xboxConfig.UseSpuIrq);
		FrameLimitCbox.SetCheck(xboxConfig.UseFrameLimiter);
		GpuThCbox.SetCheck(xboxConfig.UseThreadedGpu);
		return S_OK;
	}

	HRESULT OnRender(
		XUIMessageRender *pInputData,
		BOOL &bHandled
		) {
			swprintf(fileBrowserInfoText, L"%d/%d", FileBrowser.GetCurSel() + 1, FileBrowser.GetItemCount());
			FileBrowserInfo.SetText(fileBrowserInfoText);

			return S_OK;
	}


	HRESULT OnNotifyPress( HXUIOBJ hObjPressed, 
		BOOL& bHandled )
	{

		if (hObjPressed == FileBrowser) {
			OsdBtn.Press();
		}

		if (hObjPressed == DynarecCbox) {
			xboxConfig.UseDynarec = DynarecCbox.IsChecked();
		}

		if (hObjPressed == SpuIrqCbox) {
			xboxConfig.UseSpuIrq = SpuIrqCbox.IsChecked();
		}

		if (hObjPressed == FrameLimitCbox) {
			xboxConfig.UseFrameLimiter = FrameLimitCbox.IsChecked();
		}

		if (hObjPressed == GpuThCbox) {
			xboxConfig.UseThreadedGpu = GpuThCbox.IsChecked();
		}

		return S_OK;
	}

public:
	XUI_IMPLEMENT_CLASS( CMainMenuScene, L"MainMenu", XUI_CLASS_SCENE )
};


HRESULT CMyApp::RegisterXuiClasses()
{
	HRESULT hr = CMainMenuScene::Register();
	if( FAILED( hr ) )
		return hr;

	hr = CFileBrowserList::Register();
	if( FAILED( hr ) )
		return hr;

	hr = COsdMenuScene::Register();
	if( FAILED( hr ) )
		return hr;

	hr = CLoadStateBrowser::Register();
	if( FAILED( hr ) )
		return hr;

	hr = CEffectBrowser::Register();
	if( FAILED( hr ) )
		return hr;

	return S_OK;
}

HRESULT CMyApp::UnregisterXuiClasses()
{
	CMainMenuScene::Unregister();
	CFileBrowserList::Unregister();
	CLoadStateBrowser::Unregister();
	COsdMenuScene::Unregister();
	CEffectBrowser::Unregister();
	return S_OK;
}

