#include <xtl.h>
#include <xboxmath.h>
#include <xui.h>
#include <xuiapp.h>
#include <string>
#include <vector>


// D3d
extern void InitD3D();
extern IDirect3DDevice9*				g_pd3dDevice;
extern D3DPRESENT_PARAMETERS			g_d3dpp;

// Pcsx Helper
extern void ResetPcsx();
extern void ShutdownPcsx();
extern void RunPcsx(char * game);
extern void PausePcsx();
extern void ResumePcsx();
extern void DrawPcsxSurface();

extern void SaveStatePcsx(int n);
extern void LoadStatePcsx(int n);



// Peops
extern "C" int UseFrameLimit;



std::string get_string(const std::wstring & s, std::string & d);
std::wstring get_wstring(const std::string & s, std::wstring & d);


//----------------------------------------------------------------------------------
// Name: CXuiWaitList
// Desc: Class used to wait on handles.  When a handle becomes signalled, a 
//       user-specified action (send specified XUI message or call specified 
//       callback function) is taken.
//----------------------------------------------------------------------------------
typedef void (*PFN_WAITCOMPLETION )( void* pvContext );

class CXuiWaitList
{

protected:

    enum
    {
        WAIT_HANDLE_F_NONE      = 0,
        WAIT_HANDLE_F_NOREMOVE  = 1,
    };

    struct WaitEntry
    {
        HXUIOBJ hObj;
        DWORD dwMessageId;
        PFN_WAITCOMPLETION pfnCompletionRoutine;
        void* pvContext;
        DWORD dwFlags;
    };

    int m_nNumWaitHandles;
    HANDLE      m_WaitHandles[ MAXIMUM_WAIT_OBJECTS ];
    WaitEntry   m_WaitEntries[ MAXIMUM_WAIT_OBJECTS ];

    //------------------------------------------------------------------------------
    // Dispatches the registered entry at the specified index.
    //------------------------------------------------------------------------------
    void        DispatchWaitEntry( int nIndex );

    //------------------------------------------------------------------------------
    // Removes the specified index from the wait list.
    //------------------------------------------------------------------------------
    void        RemoveWaitEntry( int nIndex );

public:

    //------------------------------------------------------------------------------
    // Registers a handle for async operations.
    //------------------------------------------------------------------------------
    BOOL        RegisterWaitHandle( HANDLE hWait, HXUIOBJ hObj, DWORD dwMessageId, BOOL bRemoveAfterSignaled );

    //------------------------------------------------------------------------------
    // Registers a callback for when the given event fires.
    //------------------------------------------------------------------------------
    BOOL        RegisterWaitHandleFunc( HANDLE hWait, PFN_WAITCOMPLETION pfnCompletion, void* pvContext,
                                        BOOL bRemoveAfterSignaled );

    //------------------------------------------------------------------------------
    // Removes the specified handle from the wait list.
    //------------------------------------------------------------------------------
    void        UnregisterWaitHandle( HANDLE hWait );

    //------------------------------------------------------------------------------
    // Checks the list of registered wait handles.
    //------------------------------------------------------------------------------
    void        ProcessWaitHandles();

};




// Xui Main app
class CMyApp : public CXuiModule
{
public:
    CXuiWaitList m_waitlist;
protected:
    virtual HRESULT RegisterXuiClasses();
    virtual HRESULT UnregisterXuiClasses();
public:
    // Override RunFrame so that CMyApp can trap the user-defined event.
    virtual void    RunFrame();
};

// Pcsx xbox config
class XboxConfig {
public:
	std::string game;
	bool Running;
	bool ShutdownRequested;
	bool ResetRequested;
	bool OsdMenuRequested;
	//
	bool UseDynarec;
	bool UseSpuIrq;
	bool UseFrameLimiter;
	// 
	std::string saveStateDir;
};

extern XboxConfig xboxConfig;
extern CMyApp app;