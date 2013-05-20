#include <xtl.h>
#include <xboxmath.h>
#include <xui.h>
#include <xuiapp.h>
#include <string>
#include <vector>


// D3d
extern void InitD3D();
extern void VideoPresent();
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
extern void LoadStatePcsx(std::string save);

extern  void LoadShaderFromFile(const char * filename);

// Peops
extern "C" int UseFrameLimit;



std::string get_string(const std::wstring & s, std::string & d);
std::wstring get_wstring(const std::string & s, std::wstring & d);

HRESULT ShowKeyboard(std::wstring & resultText, std::wstring titleText = L"", std::wstring descriptionText = L"", std::wstring defaultText = L"");

// Xui Main app
class CMyApp : public CXuiModule
{
protected:
    virtual HRESULT RegisterXuiClasses();
    virtual HRESULT UnregisterXuiClasses();
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
	bool UseThreadedGpu;
	bool UseFrameLimiter;
	// 
	int region;
	std::string saveStateDir;
};

extern XboxConfig xboxConfig;
extern CMyApp app;