#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <string>
#include <vector>

void RunPcsx(char * game);

// Global string table for this application.
CXuiStringTable StringTable;

std::string get_string(const std::wstring & s, std::string & d)
{
	char buffer[256];
	const wchar_t * cs = s.c_str();
	wcstombs ( buffer, cs, sizeof(buffer) );

	d = buffer;

	return d;
}

std::wstring get_wstring(const std::string & s, std::wstring & d)
{
	wchar_t buffer[256];
	const char * cs = s.c_str();
	mbstowcs ( buffer, cs, sizeof(buffer) );

	d = buffer;

	return d;
}


class FileBrowserProvider {
protected:
	struct _FILE_INFO {
		std::wstring filename;
		std::wstring displayname;
		bool isDir;
	};

	std::wstring currentDir;
	std::vector<_FILE_INFO> fileList;
	 
protected:
	
public:
	void Init() {
		currentDir = L"game:";
	}

	void AddParentEntry(std::wstring currentDir) {
		_FILE_INFO finfo;
		unsigned int p = currentDir.rfind(L"\\");
		currentDir = currentDir.substr(0,  p);

		finfo.isDir = true;
		finfo.displayname = L"..";
		finfo.filename = currentDir;

		fileList.push_back(finfo);
	}

	HRESULT UpdateDirList(std::wstring currentDir = L"game:") {
		WIN32_FIND_DATA ffd;
		LARGE_INTEGER filesize;
		std::string szDir;
		size_t length_of_arg;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		DWORD dwError=0;		
		
		// Free file list
		fileList.clear();

		AddParentEntry(currentDir);

		get_string(currentDir, szDir);
		szDir = szDir + "\\*";

		// Look for files in current directory
		hFind = FindFirstFile(szDir.c_str(), &ffd);

		if (INVALID_HANDLE_VALUE == hFind) 
		{
			return dwError;
		} 

		// List all the files in the directory with some info about them.
		do
		{
			_FILE_INFO finfo;
			finfo.isDir = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			get_wstring(ffd.cFileName, finfo.displayname);
			finfo.filename = currentDir + L"\\" + finfo.displayname;

			fileList.push_back(finfo);

		}
		while (FindNextFile(hFind, &ffd) != 0);

		dwError = GetLastError();

		FindClose(hFind);

		return S_OK;
	}

	DWORD Size() {
		return this->fileList.size();
	}

	LPCWSTR At(int i) {
		if (i >= 0 & i < Size()) {
			return this->fileList[i].displayname.c_str();
		} else {
			return L"";
		}
	}

	LPCWSTR Filename(int i) {
		if (i >= 0 & i < Size()) {
			return this->fileList[i].filename.c_str();
		} else {
			return L"";
		}
	}

	bool IsDir(int i) {
		if (i >= 0 & i < Size()) {
			return this->fileList[i].isDir;
		} else {
			return false;
		}
	}
};


FileBrowserProvider fileList;


//--------------------------------------------------------------------------------------
// Name: class CLanguageList
// Desc: List implementation class.
//--------------------------------------------------------------------------------------
class CFileBrowserList : public CXuiListImpl
{
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
        fileList.Init();
		fileList.UpdateDirList();

        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Returns the number of items in the list.
    //----------------------------------------------------------------------------------
    HRESULT OnGetItemCountAll( XUIMessageGetItemCount* pGetItemCountData, BOOL& bHandled )
    {
		pGetItemCountData->cItems = fileList.Size();
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
			pGetSourceTextData->szText = fileList.At(pGetSourceTextData->iItem);
            bHandled = TRUE;
        }
        return S_OK;
    }

	HRESULT OnNotifyPress( HXUIOBJ hObjPressed, 
		BOOL& bHandled )
	{
		int nIndex;

		// Change the current dir
		if (fileList.IsDir(GetCurSel())) {
			fileList.UpdateDirList(fileList.Filename(GetCurSel()));
			// Move to top
			SetCurSel(0);
		} else {
			std::string game;			
			get_string(fileList.Filename(GetCurSel()), game);
			char * cgame = (char*)game.c_str();
			RunPcsx(cgame);
		}
		
		bHandled = TRUE;
		return S_OK;
	}

protected:
	
public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CFileBrowserList, L"FileBrowserList", XUI_CLASS_LIST )
};


class CMyMainScene : public CXuiSceneImpl
{
    CXuiList FileBrowser;


    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
    XUI_END_MSG_MAP()


    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retrieves controls.
    //----------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
		GetChildById( L"FileBrowser", &FileBrowser );
        return S_OK;
    }


public:
    XUI_IMPLEMENT_CLASS( CMyMainScene, L"MainMenu", XUI_CLASS_SCENE )
};

class CMyApp : public CXuiModule
{
protected:
    virtual HRESULT RegisterXuiClasses();
    virtual HRESULT UnregisterXuiClasses();
};


HRESULT CMyApp::RegisterXuiClasses()
{
    HRESULT hr = CMyMainScene::Register();
    if( FAILED( hr ) )
        return hr;

	hr = CFileBrowserList::Register();
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}

HRESULT CMyApp::UnregisterXuiClasses()
{
    CMyMainScene::Unregister();
    return S_OK;
}

extern void InitD3D();

extern IDirect3DDevice9*				g_pd3dDevice;
extern D3DPRESENT_PARAMETERS			g_d3dpp;




VOID __cdecl gui_main()
{
	InitD3D();


    // Declare an instance of the XUI framework application.
    CMyApp app;

    // Initialize the application.    
    // HRESULT hr = app.Init( XuiD3DXTextureLoader );
	HRESULT hr = app.InitShared(g_pd3dDevice, &g_d3dpp, XuiD3DXTextureLoader, NULL);

    // Register a default typeface
    hr = app.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xui/xarialuni.ttf" );


    // Load the skin file used for the scene. 
    app.LoadSkin( L"file://game:/media/xui/simple_scene_skin.xur" );

    // Load the scene.
    app.LoadFirstScene( L"file://game:/media/xui/", L"scene.xur", NULL );

    // Run the scene using the built-in loop.
    //app.Run();

	while(1) {
		g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
		0xff00FF00, 1.0f, 0L );

		app.RunFrame();
		app.Render();
		XuiTimersRun();

		// Present the backbuffer contents to the display.
		g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
	}

    // Free resources, unregister custom classes, and exit.
    app.Uninit();
}

