#include "quakedef.h"
#ifdef D3D11QUAKE
#include "winquake.h"
#include "gl_draw.h"
#include "glquake.h"
#include "shader.h"
#include "renderque.h"

#define COBJMACROS
#include <d3d11.h>

/*Fixup outdated windows headers*/
#ifndef WM_XBUTTONDOWN
   #define WM_XBUTTONDOWN      0x020B
   #define WM_XBUTTONUP      0x020C
#endif
#ifndef MK_XBUTTON1
   #define MK_XBUTTON1         0x0020
#endif
#ifndef MK_XBUTTON2
   #define MK_XBUTTON2         0x0040
#endif
// copied from DarkPlaces in an attempt to grab more buttons
#ifndef MK_XBUTTON3
   #define MK_XBUTTON3         0x0080
#endif
#ifndef MK_XBUTTON4
   #define MK_XBUTTON4         0x0100
#endif
#ifndef MK_XBUTTON5
   #define MK_XBUTTON5         0x0200
#endif
#ifndef MK_XBUTTON6
   #define MK_XBUTTON6         0x0400
#endif
#ifndef MK_XBUTTON7
   #define MK_XBUTTON7         0x0800
#endif

#ifndef WM_INPUT
	#define WM_INPUT 255
#endif

#define DEFINE_QGUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

DEFINE_QGUID(qIID_ID3D11Texture2D,0x6f15aaf2,0xd208,0x4e89,0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c);

ID3D11Device *pD3DDev11;
ID3D11DeviceContext *d3ddevctx;
IDXGISwapChain *d3dswapchain;

ID3D11RenderTargetView *fb_backbuffer;
ID3D11DepthStencilView *fb_backdepthstencil;

void *d3d11mod;

qboolean vid_initializing;

extern qboolean		scr_initialized;                // ready to draw
extern qboolean		scr_drawloading;
extern qboolean		scr_con_forcedraw;
static qboolean d3d_resized;

cvar_t vid_hardwaregamma;


//sound/error code needs this
HWND mainwindow;

//input code needs these
int		window_center_x, window_center_y;
RECT		window_rect;
int window_x, window_y;

static void released3dbackbuffer(void);
static qboolean resetd3dbackbuffer(int width, int height);

#if 0//def _DEBUG
#include <dxgidebug.h>
const GUID IID_IDXGIDebug = { 0x119E7452,0xDE9E,0x40fe, { 0x88,0x06,0x88,0xF9,0x0C,0x12,0xB4,0x41 } };

const GUID DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, {0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8 }};

void DoDXGIDebug(void)
{
	IDXGIDebug *dbg = NULL;

	HRESULT (WINAPI *pDXGIGetDebugInterface)(REFIID riid, void **ppDebug);
	dllfunction_t dxdidebugfuncs[] =
	{
		{(void**)&pDXGIGetDebugInterface, "DXGIGetDebugInterface"},
		{NULL}
	};
	pDXGIGetDebugInterface = NULL;
	Sys_LoadLibrary("dxgidebug", dxdidebugfuncs);
	pDXGIGetDebugInterface(&IID_IDXGIDebug, &dbg);
	if (dbg)
	{
		IDXGIDebug_ReportLiveObjects(dbg, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		IDXGIDebug_Release(dbg);
	}
}
#else
#define DoDXGIDebug()
#endif

char *D3D_NameForResult(HRESULT hr)
{
	if (hr == DXGI_ERROR_DEVICE_REMOVED && pD3DDev11)
		hr = ID3D11Device_GetDeviceRemovedReason(pD3DDev11);

	switch(hr)
	{
	case E_OUTOFMEMORY:						return "E_OUTOFMEMORY";
	case E_NOINTERFACE:						return "E_NOINTERFACE";
	case DXGI_ERROR_DEVICE_HUNG:			return "DXGI_ERROR_DEVICE_HUNG";
	case DXGI_ERROR_DEVICE_REMOVED:			return "DXGI_ERROR_DEVICE_REMOVED";
	case DXGI_ERROR_DEVICE_RESET:			return "DXGI_ERROR_DEVICE_RESET";
	case DXGI_ERROR_DRIVER_INTERNAL_ERROR:	return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
	case DXGI_ERROR_INVALID_CALL:			return "DXGI_ERROR_INVALID_CALL";
	default:								return va("%x", hr);
	}
}

static void D3D11_PresentOrCrash(void)
{
	extern cvar_t _vid_wait_override;
	HRESULT hr = IDXGISwapChain_Present(d3dswapchain, _vid_wait_override.ival, 0);
	if (FAILED(hr))
		Sys_Error("IDXGISwapChain_Present: %s\n", D3D_NameForResult(hr));
}

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;
static modestate_t modestate;


static void D3DVID_UpdateWindowStatus (HWND hWnd)
{
	POINT p;
	RECT nr;
	int window_width, window_height;
	GetClientRect(hWnd, &nr);

	Sys_Printf("Update: %i %i %i %i\n", nr.left, nr.top, nr.right, nr.bottom);

	//if its bad then we're probably minimised
	if (nr.right <= nr.left)
		return;
	if (nr.bottom <= nr.top)
		return;

	p.x = 0;
	p.y = 0;
	ClientToScreen(hWnd, &p);
	window_x = p.x;
	window_y = p.y;
	window_width = nr.right - nr.left;
	window_height = nr.bottom - nr.top;
//	vid.pixelwidth = window_width;
//	vid.pixelheight = window_height;

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	Sys_Printf("Window: %i %i %i %i\n", window_x, window_y, window_width, window_height);


	INS_UpdateClipCursor ();
}

static qboolean D3D11AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	if (ActiveApp == fActive && Minimized == minimize)
		return false;	//so windows doesn't crash us over and over again.

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	INS_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);

	if (fActive)
	{
		Cvar_ForceCallback(&v_gamma);
	}
	if (!fActive)
	{
		Cvar_ForceCallback(&v_gamma);	//wham bam thanks.
	}

	return true;
}





static LRESULT WINAPI D3D11_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG    lRet = 0;
	int		fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;
	extern qboolean	keydown[K_MAX];

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

	switch (uMsg)
	{
#if 1
/*		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;
*/
//		case WM_CREATE:
//			break;

		case WM_MOVE:
			D3DVID_UpdateWindowStatus (hWnd);
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (keydown[K_LALT] && wParam == '\r')
			{
				if (modestate == MS_FULLSCREEN)
					modestate = MS_WINDOWED;
				else
				{
					RECT rect;
					extern cvar_t vid_width, vid_height;
					int width = vid_width.ival;
					int height = vid_height.ival;
					rect.left = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
					rect.top = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
					rect.right = rect.left+width;
					rect.bottom = rect.top+height;
					AdjustWindowRectEx(&rect, WS_OVERLAPPED, FALSE, 0);
					SetWindowPos(hWnd, NULL, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, SWP_SHOWWINDOW|SWP_FRAMECHANGED);
					modestate = MS_FULLSCREEN;
				}
				IDXGISwapChain_SetFullscreenState(d3dswapchain, modestate == MS_FULLSCREEN, NULL);

				if (modestate == MS_WINDOWED)
				{
					RECT rect;
					int width = 640;
					int height = 480;
					rect.left = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
					rect.top = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
					rect.right = rect.left+width;
					rect.bottom = rect.top+height;
					AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
					SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);	//make sure dxgi didn't break us.
					SetWindowPos(hWnd, HWND_TOP, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, SWP_SHOWWINDOW|SWP_FRAMECHANGED);
					SetForegroundWindow(hWnd);
					SetFocus(hWnd);
				}
			}
			else if (!vid_initializing)
				INS_TranslateKeyEvent (wParam, lParam, true, 0);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!vid_initializing)
				INS_TranslateKeyEvent (wParam, lParam, false, 0);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

			if (wParam & MK_XBUTTON3)
				temp |= 32;

			if (wParam & MK_XBUTTON4)
				temp |= 64;

			if (wParam & MK_XBUTTON5)
				temp |= 128;

			if (wParam & MK_XBUTTON6)
				temp |= 256;

			if (wParam & MK_XBUTTON7)
				temp |= 512;

			if (!vid_initializing)
				INS_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL:
			if (!vid_initializing)
			{
				if ((short) HIWORD(wParam) > 0)
				{
					Key_Event(0, K_MWHEELUP, 0, true);
					Key_Event(0, K_MWHEELUP, 0, false);
				}
				else
				{
					Key_Event(0, K_MWHEELDOWN, 0, true);
					Key_Event(0, K_MWHEELDOWN, 0, false);
				}
			}
			break;

		case WM_INPUT:
			// raw input handling
			if (!vid_initializing)
				INS_RawInput_Read((HANDLE)lParam);
			break;

		case WM_GETMINMAXINFO:
			{
				RECT windowrect;
				RECT clientrect;
				MINMAXINFO *mmi = (MINMAXINFO *) lParam;

				GetWindowRect (hWnd, &windowrect);
				GetClientRect (hWnd, &clientrect);

				mmi->ptMinTrackSize.x = 320 + ((windowrect.right - windowrect.left) - (clientrect.right - clientrect.left));
				mmi->ptMinTrackSize.y = 200 + ((windowrect.bottom - windowrect.top) - (clientrect.bottom - clientrect.top));
			}
			return 0;
    	case WM_SIZE:
			d3d_resized = true;

			D3DVID_UpdateWindowStatus(mainwindow);

			released3dbackbuffer();
			IDXGISwapChain_ResizeBuffers(d3dswapchain, 0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

			D3D11BE_Reset(true);
			vid.pixelwidth = window_rect.right - window_rect.left;
			vid.pixelheight = window_rect.bottom - window_rect.top;
			resetd3dbackbuffer(vid.pixelwidth, vid.pixelheight);
			D3D11BE_Reset(false);
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;

		case WM_CLOSE:
			if (!vid_initializing)
				if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
							MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
				{
					Cbuf_AddText("\nquit\n", RESTRICT_LOCAL);
				}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			if (!D3D11AppActivate(!(fActive == WA_INACTIVE), fMinimized))
				break;//so, urm, tell me microsoft, what changed?

			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWNORMAL);

			if (ActiveApp && modestate == MS_FULLSCREEN)
				IDXGISwapChain_SetFullscreenState(d3dswapchain, modestate == MS_FULLSCREEN, NULL);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
//			ClearAllStates ();

			lRet = 1;
			break;

   	    case WM_DESTROY:
        {
//			if (dibwindow)
//				DestroyWindow (dibwindow);
        }
        break;

		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;
#endif
		case WM_ERASEBKGND:
			return 1;
    	default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }

    /* return 1 if handled message, 0 if not */
    return lRet;
}

#if (WINVER < 0x500) && !defined(__GNUC__)
typedef struct tagMONITORINFO
{
    DWORD   cbSize;
    RECT    rcMonitor;
    RECT    rcWork;
    DWORD   dwFlags;
} MONITORINFO, *LPMONITORINFO;
#endif

static void released3dbackbuffer(void)
{
	if (d3ddevctx)
		ID3D11DeviceContext_OMSetRenderTargets(d3ddevctx, 0, NULL, NULL);
	if (fb_backbuffer)
		ID3D11RenderTargetView_Release(fb_backbuffer);
	fb_backbuffer = NULL;
	if (fb_backdepthstencil)
		ID3D11DepthStencilView_Release(fb_backdepthstencil);
	fb_backdepthstencil = NULL;
}

static qboolean resetd3dbackbuffer(int width, int height)
{
	D3D11_TEXTURE2D_DESC t2ddesc;
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
	ID3D11Texture2D *backbuftex, *depthtex;

	released3dbackbuffer();

	//get a proper handle to the backbuffer (silly hurdles)
	if (FAILED(IDXGISwapChain_GetBuffer(d3dswapchain, 0, &qIID_ID3D11Texture2D, (LPVOID*)&backbuftex)))
		return false;
	if (FAILED(ID3D11Device_CreateRenderTargetView(pD3DDev11, (ID3D11Resource*)backbuftex, NULL, &fb_backbuffer)))
		return false;
	ID3D11Texture2D_Release(backbuftex);

	//set up a depth buffer.
	memset(&t2ddesc, 0, sizeof(t2ddesc));
	t2ddesc.Width = width;
	t2ddesc.Height = height;
	t2ddesc.MipLevels = 1;
	t2ddesc.ArraySize = 1;
	t2ddesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	t2ddesc.SampleDesc.Count = 1;
	t2ddesc.SampleDesc.Quality = 0;
	t2ddesc.Usage = D3D11_USAGE_DEFAULT;
	t2ddesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	t2ddesc.CPUAccessFlags = 0;
	t2ddesc.MiscFlags = 0;
	if(FAILED(ID3D11Device_CreateTexture2D(pD3DDev11, &t2ddesc, NULL, &depthtex)))
		return false;
	dsvd.Format = t2ddesc.Format;
	dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvd.Texture2D.MipSlice = 0;
	if(FAILED(ID3D11Device_CreateDepthStencilView(pD3DDev11, (ID3D11Resource*)depthtex, NULL/*&dsvd*/, &fb_backdepthstencil)))
		return false;
	ID3D11Texture2D_Release(depthtex);

	//now tell d3d which render targets to use.
	ID3D11DeviceContext_OMSetRenderTargets(d3ddevctx, 1, &fb_backbuffer, fb_backdepthstencil);

	return true;
}

static qboolean initD3D11Device(HWND hWnd, rendererstate_t *info, PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN func, IDXGIAdapter *adapt)
{
	int flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
	D3D_DRIVER_TYPE drivertype;
	DXGI_SWAP_CHAIN_DESC scd;
	D3D_FEATURE_LEVEL flevel, flevels[] =
	{
		//D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,

		//FIXME: need npot.
//		D3D_FEATURE_LEVEL_9_3,
//		D3D_FEATURE_LEVEL_9_2,
//		D3D_FEATURE_LEVEL_9_1
	};
	memset(&scd, 0, sizeof(scd));

	if (!stricmp(info->subrenderer, "debug"))
		flags |= D3D11_CREATE_DEVICE_DEBUG;

	if (!stricmp(info->subrenderer, "warp"))
		drivertype = D3D_DRIVER_TYPE_WARP;
	else if (!stricmp(info->subrenderer, "ref"))
		drivertype = D3D_DRIVER_TYPE_REFERENCE;
	else if (!stricmp(info->subrenderer, "hw"))
		drivertype = D3D_DRIVER_TYPE_HARDWARE;
	else if (!stricmp(info->subrenderer, "null"))
		drivertype = D3D_DRIVER_TYPE_NULL;
	else if (!stricmp(info->subrenderer, "software"))
		drivertype = D3D_DRIVER_TYPE_SOFTWARE;
	else if (!stricmp(info->subrenderer, "unknown"))
		drivertype = D3D_DRIVER_TYPE_UNKNOWN;
	else
		drivertype = adapt?D3D_DRIVER_TYPE_UNKNOWN:D3D_DRIVER_TYPE_HARDWARE;

	//for stereo support, we would have to rewrite all of this in a way that would make us dependant upon windows 8 or 7+platform update, which would exclude vista.
	scd.BufferDesc.Width = info->width;
	scd.BufferDesc.Height = info->height;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 0;
	scd.BufferCount = 1+info->triplebuffer;	//back buffer count
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	//32bit colour
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = 1+info->multisample;
	scd.Windowed = TRUE;
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;// | DXGI_SWAP_CHAIN_FLAG_NONPREROTATED;

#ifdef _DEBUG
//	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	if (drivertype == D3D_DRIVER_TYPE_UNKNOWN && adapt)
	{
		DXGI_ADAPTER_DESC adesc;
		IDXGIAdapter_GetDesc(adapt, &adesc);
		Con_Printf("D3D11 Adaptor: %S\n", adesc.Description);
	}
	else
		adapt = NULL;

	if (FAILED(func(adapt, drivertype, NULL, flags,
				flevels, sizeof(flevels)/sizeof(flevels[0]),
				D3D11_SDK_VERSION,
				&scd,
				&d3dswapchain,
				&pD3DDev11,
				&flevel,
				&d3ddevctx)))
		return false;

	if (!pD3DDev11)
		return false;

	Con_Printf("D3D11 Feature level: %i_%i\n", flevel>>12, (flevel>>8) & 0xf);

	if (!resetd3dbackbuffer(info->width, info->height))
		return false;

	if (info->fullscreen)
	{
	}

	vid.numpages = scd.BufferCount;
	if (!D3D11Shader_Init(flevel))
	{
		Con_Printf("Unable to intialise a suitable HLSL compiler, please install the DirectX runtime.\n");
		return false;
	}
	return true;
}

static void initD3D11(HWND hWnd, rendererstate_t *info)
{
	static dllhandle_t *d3d11dll;
	static dllhandle_t *dxgi;
	static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN fnc;
	static HRESULT (WINAPI *pCreateDXGIFactory1)(IID *riid, void **ppFactory);
	static IID factiid = {0x770aae78, 0xf26f, 0x4dba, 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87};
	IDXGIFactory1 *fact = NULL;
	IDXGIAdapter *adapt = NULL;
	dllfunction_t d3d11funcs[] =
	{
		{(void**)&fnc, "D3D11CreateDeviceAndSwapChain"},
		{NULL}
	};
	dllfunction_t dxgifuncs[] =
	{
		{(void**)&pCreateDXGIFactory1, "CreateDXGIFactory1"},
		{NULL}
	};

	if (!dxgi)
		dxgi = Sys_LoadLibrary("dxgi", dxgifuncs);
	if (!d3d11mod)
		d3d11mod = Sys_LoadLibrary("d3d11", d3d11funcs);

	if (!d3d11mod)
		return;

	if (pCreateDXGIFactory1)
	{
		HRESULT hr;
		hr = pCreateDXGIFactory1(&factiid, &fact);
		if (FAILED(hr))
			Con_Printf("CreateDXGIFactory1 failed: %s\n", D3D_NameForResult(hr));
		if (fact)
		{
			IDXGIFactory1_EnumAdapters(fact, 0, &adapt);
		}
	}

	
	initD3D11Device(hWnd, info, fnc, adapt);

	if (adapt)
		IDXGIAdapter_Release(adapt);
	if (fact)
	{
		//DXGI SUCKS and fucks up alt+tab every single time. its pointless to go from fullscreen to fullscreen-with-taskbar-obscuring-half-the-window.
		//I'm just going to handle that stuff myself.
		IDXGIFactory1_MakeWindowAssociation(fact, hWnd, DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_ALT_ENTER|DXGI_MWA_NO_PRINT_SCREEN);
		IDXGIFactory1_Release(fact);
	}
}

static qboolean D3D11_VID_Init(rendererstate_t *info, unsigned char *palette)
{
	DWORD width = info->width;
	DWORD height = info->height;
	//DWORD bpp = info->bpp;
	//DWORD zbpp = 16;
	//DWORD flags = 0;
	DWORD wstyle;
	RECT rect;
	MSG msg;

	extern cvar_t vid_conwidth;
	//extern cvar_t vid_conheight;

	//DDGAMMARAMP gammaramp;
	//int i;

	char *CLASSNAME = "FTED3D11QUAKE";
	WNDCLASS wc = {
		0,
		&D3D11_WindowProc,
		0,
		0,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		CLASSNAME
	};

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hInstance = global_hInstance; 

	vid_initializing = true;

	RegisterClass(&wc);

	modestate = info->fullscreen?MS_FULLSCREEN:MS_WINDOWED;

	wstyle = WS_OVERLAPPEDWINDOW;

	rect.left = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	rect.top = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	rect.right = rect.left+width;
	rect.bottom = rect.top+height;
	AdjustWindowRectEx(&rect, wstyle, FALSE, 0);
	mainwindow = CreateWindow(CLASSNAME, "Direct3D11", wstyle, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, NULL, NULL, NULL, NULL);

	// Try as specified.

	DoDXGIDebug();

	initD3D11(mainwindow, info);
	if (!pD3DDev11)
	{
		DoDXGIDebug();
		Con_Printf("No suitable D3D11 device found\n");
		return false;
	}

	if (info->fullscreen)
		IDXGISwapChain_SetFullscreenState(d3dswapchain, true, NULL);

	vid.pixelwidth = width;
	vid.pixelheight = height;

	while (PeekMessage(&msg, NULL,  0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	CL_UpdateWindowTitle();

	ShowWindow(mainwindow, SW_SHOWNORMAL);

	vid.width = vid.pixelwidth;
	vid.height = vid.pixelheight;

	vid_initializing = false;

	GetWindowRect(mainwindow, &window_rect);

	{
		extern qboolean	mouseactive;
		mouseactive = false;
	}

	Cvar_ForceCallback(&v_gamma);

	return true;
}

/*a new model has been loaded*/
static void	(D3D11_R_NewMap)					(void)
{
	r_worldentity.model = cl.worldmodel;

#ifdef MAP_PROC
	if (cl.worldmodel && cl.worldmodel->fromgame == fg_doom3)
		D3_GenerateAreas(cl.worldmodel);
#endif

	/*wipe any lingering particles*/
	P_ClearParticles();
	CL_RegisterParticles();

	R_AnimateLight();
	Surf_DeInit();
	Surf_WipeStains();
	Surf_BuildLightmaps();

	TP_NewMap();
	R_SetSky(cl.skyname);

#ifdef RTLIGHTS
	Sh_PreGenerateLights();
#endif
}

extern mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
static void	(D3D11_R_PreNewMap)				(void)
{
	r_viewleaf = NULL;
	r_oldviewleaf = NULL;
	r_viewleaf2 = NULL;
	r_oldviewleaf2 = NULL;
}

static void	 (D3D11_VID_DeInit)				(void)
{
	D3D11BE_Shutdown();

	/*we cannot shut down cleanly while in fullscreen, supposedly*/
	if(d3dswapchain)
		IDXGISwapChain_SetFullscreenState(d3dswapchain, false, NULL);

	released3dbackbuffer();
	if(d3dswapchain)
		IDXGISwapChain_Release(d3dswapchain);
	d3dswapchain = NULL;
	if (pD3DDev11)
		ID3D11Device_Release(pD3DDev11);
	pD3DDev11 = NULL;

	if (d3ddevctx)
	{
		ID3D11DeviceContext_ClearState(d3ddevctx);
		ID3D11DeviceContext_Flush(d3ddevctx);
		ID3D11DeviceContext_Release(d3ddevctx);
	}
	d3ddevctx = NULL;

	if (mainwindow)
	{
		DestroyWindow(mainwindow);
		mainwindow = NULL;
	}

	DoDXGIDebug();
}

static qboolean	D3D11_VID_ApplyGammaRamps(unsigned short *ramps)
{
	return false;
}
static char	*D3D11_VID_GetRGBInfo(int prepad, int *truevidwidth, int *truevidheight)
{
	//don't directly map the frontbuffer, as that can hold other things.
	//create a texture, copy the (gpu)backbuffer to that (cpu)texture
	//then map the (cpu)texture and copy out the parts we need, reordering as needed.
	D3D11_MAPPED_SUBRESOURCE lock;
	qbyte *rgb, *in, *r = NULL;
	unsigned int x,y;
	D3D11_TEXTURE2D_DESC texDesc;
	ID3D11Texture2D *texture;
	ID3D11Resource *backbuffer;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = 0;
	texDesc.CPUAccessFlags = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = vid.pixelwidth;
	texDesc.Height = vid.pixelheight;
	texDesc.MipLevels = 1;
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	if (FAILED(ID3D11Device_CreateTexture2D(pD3DDev11, &texDesc, 0, &texture)))
		return NULL;
	ID3D11RenderTargetView_GetResource(fb_backbuffer, &backbuffer);
	ID3D11DeviceContext_CopyResource(d3ddevctx, (ID3D11Resource*)texture, backbuffer);
	ID3D11Resource_Release(backbuffer);
	if (!FAILED(ID3D11DeviceContext_Map(d3ddevctx, (ID3D11Resource*)texture, 0, D3D11_MAP_READ, 0, &lock)))
	{
		r = rgb = BZ_Malloc(prepad + 3 * vid.pixelwidth * vid.pixelheight);
		r += prepad;
		for (y = vid.pixelheight; y-- > 0; )
		{
			in = lock.pData;
			in += y * lock.RowPitch;
			for (x = 0; x < vid.pixelwidth; x++, rgb+=3, in+=4)
			{
				rgb[0] = in[0];
				rgb[1] = in[1];
				rgb[2] = in[2];
			}
		}
		ID3D11DeviceContext_Unmap(d3ddevctx, (ID3D11Resource*)texture, 0);
	}
	ID3D11Texture2D_Release(texture);
	*truevidwidth = vid.pixelwidth;
	*truevidheight = vid.pixelheight;
	return r;
}
static void	(D3D11_VID_SetWindowCaption)		(char *msg)
{
	SetWindowText(mainwindow, msg);
}

void D3D11_Set2D (void)
{
	D3D11_VIEWPORT vport;

//	Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0 + (0.5*vid.width/vid.pixelwidth), vid.width + (0.5*vid.width/vid.pixelwidth), 0 + (0.5*vid.height/vid.pixelheight), vid.height + (0.5*vid.height/vid.pixelheight), 0, 100);
	Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0, vid.width, vid.height, 0, 0, 99999);
	Matrix4x4_Identity(r_refdef.m_view);

	vport.TopLeftX = 0;
	vport.TopLeftY = 0;
	vport.Width = vid.pixelwidth;
	vport.Height = vid.pixelheight;
	vport.MinDepth = 0;
	vport.MaxDepth = 1;

    ID3D11DeviceContext_RSSetViewports(d3ddevctx, 1, &vport);
	D3D11BE_SetupViewCBuffer();
}

static void	(D3D11_SCR_UpdateScreen)			(void)
{
	//extern int keydown[];
	//extern cvar_t vid_conheight;
	int uimenu;
#ifdef TEXTEDITOR
	//extern qboolean editormodal;
#endif
	qboolean nohud, noworld;
	RSpeedMark();

	if (r_clear.ival)
	{
		float colours[4] = {1, 0, 0, 0};
		ID3D11DeviceContext_ClearRenderTargetView(d3ddevctx, fb_backbuffer, colours);
	}

	if (d3d_resized)
	{
		extern cvar_t vid_conautoscale, vid_conwidth;
		d3d_resized = false;

		// force width/height to be updated
		//vid.pixelwidth = window_rect.right - window_rect.left;
		//vid.pixelheight = window_rect.bottom - window_rect.top;
		Cvar_ForceCallback(&vid_conautoscale);
		Cvar_ForceCallback(&vid_conwidth);
	}

	if (scr_disabled_for_loading)
	{
		extern float scr_disabled_time;
		if (Sys_DoubleTime() - scr_disabled_time > 60 || Key_Dest_Has(~kdm_game))
		{
			scr_disabled_for_loading = false;
		}
		else
		{
//			IDirect3DDevice9_BeginScene(pD3DDev9);
			scr_drawloading = true;
			SCR_DrawLoading ();
			scr_drawloading = false;
//			IDirect3DDevice9_EndScene(pD3DDev9);
			D3D11_PresentOrCrash();
			RSpeedEnd(RSPEED_TOTALREFRESH);
			return;
		}
	}

	if (!scr_initialized || !con_initialized)
	{
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;                         // not initialized yet
	}

	Shader_DoReload();

#ifdef VM_UI
	uimenu = UI_MenuState();
#else
	uimenu = 0;
#endif

//	d3d11error(IDirect3DDevice9_BeginScene(pD3DDev9));
/*
#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		V_UpdatePalette (false);
#if defined(_WIN32) && defined(GLQUAKE)
		Media_RecordFrame();
#endif
		R2D_BrightenScreen();

		if (key_dest == key_console)
			Con_DrawConsole(vid_conheight.value/2, false);
		GL_EndRendering ();
		GL_DoSwap();
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}
#endif
*/
	if (Media_ShowFilm())
	{
		M_Draw(0);
//		V_UpdatePalette (false);
#if defined(_WIN32)
		Media_RecordFrame();
#endif
//		R2D_BrightenScreen();
//		IDirect3DDevice9_EndScene(pD3DDev9);
		D3D11_PresentOrCrash();
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;
	}

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	noworld = false;
	nohud = false;

#ifdef VM_CG
	if (CG_Refresh())
		nohud = true;
	else
#endif
#ifdef CSQC_DAT
		if (CSQC_DrawView())
		nohud = true;
	else
#endif
		if (uimenu != 1)
		{
			if (r_worldentity.model && cls.state == ca_active)
 				V_RenderView ();
			else
			{
				noworld = true;
			}
		}

	D3D11_Set2D();

	if (!noworld)
	{
		R2D_PolyBlend ();
		R2D_BrightenScreen();
	}

	scr_con_forcedraw = false;
	if (noworld)
	{
		if ((!Key_Dest_Has(~(kdm_game|kdm_console))) && SCR_GetLoadingStage() == LS_NONE)
			scr_con_current = vid.height;

		if (scr_con_current != vid.height)
			R2D_ConsoleBackground(0, vid.height, true);
		else
			scr_con_forcedraw = true;

		nohud = true;
	}
	else if (!nohud)
		SCR_TileClear ();

	SCR_DrawTwoDimensional(uimenu, nohud);

	V_UpdatePalette (false);

#if defined(_WIN32) && defined(GLQUAKE)
	Media_RecordFrame();
#endif

	RSpeedEnd(RSPEED_TOTALREFRESH);
	RSpeedShow();

	D3D11_PresentOrCrash();

	window_center_x = (window_rect.left + window_rect.right)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;


	INS_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);
}







static void	(D3D11_Draw_Init)				(void)
{
	R2D_Init();
}
static void	(D3D11_Draw_Shutdown)				(void)
{
	R2D_Shutdown();
}

static void	(D3D11_R_Init)					(void)
{
}
static void	(D3D11_R_DeInit)					(void)
{
	Surf_DeInit();
	Shader_Shutdown();
	D3D11_Image_Shutdown();
}



static void D3D11_SetupViewPort(void)
{
	extern cvar_t gl_mindist;
	float	screenaspect;
	int		x, x2, y2, y, w, h;

	float fov_x, fov_y;

//	D3DVIEWPORT9 vport;

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);

	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * vid.pixelwidth/(int)vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * vid.pixelwidth/(int)vid.width;
	y = (r_refdef.vrect.y) * vid.pixelheight/(int)vid.height;
	y2 = ((int)(r_refdef.vrect.y + r_refdef.vrect.height)) * vid.pixelheight/(int)vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < vid.pixelwidth)
		x2++;
	if (y < 0)
		y--;
	if (y2 < vid.pixelheight)
		y2++;

	w = x2 - x;
	h = y2 - y;

	fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
	fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

	if (r_waterwarp.value<0 && r_viewcontents & FTECONTENTS_FLUID)
	{
		fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
		fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
	}

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;

	/*view matrix*/
	Matrix4x4_CM_ModelViewMatrixFromAxis(r_refdef.m_view, vpn, vright, vup, r_refdef.vieworg);
	Matrix4x4_CM_Projection_Inf(r_refdef.m_projection, fov_x, fov_y, gl_mindist.value);
}

static void	(D3D11_R_RenderView)				(void)
{
	float x, x2, y, y2;

	D3D11_SetupViewPort();
	//unlike gl, we clear colour beforehand, because that seems more sane.
	//always clear depth
	ID3D11DeviceContext_ClearDepthStencilView(d3ddevctx, fb_backdepthstencil, D3D11_CLEAR_DEPTH, 1, 0);	//is it faster to clear the stencil too?

	x = (r_refdef.vrect.x * (int)vid.pixelwidth)/(int)vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * (int)vid.pixelwidth/(int)vid.width;
	y = (r_refdef.vrect.y * (int)vid.pixelheight)/(int)vid.height;
	y2 = (r_refdef.vrect.y + r_refdef.vrect.height) * (int)vid.pixelheight/(int)vid.height;
	r_refdef.pxrect.x = floor(x);
	r_refdef.pxrect.y = floor(y);
	r_refdef.pxrect.width = (int)ceil(x2) - r_refdef.pxrect.x;
	r_refdef.pxrect.height = (int)ceil(y2) - r_refdef.pxrect.y;

	R_SetFrustum (r_refdef.m_projection, r_refdef.m_view);
	RQ_BeginFrame();
//	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
//	{
//		if (cl.worldmodel)
//			P_DrawParticles ();
//	}
	
	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
		if (!r_worldentity.model || r_worldentity.model->needload || !cl.worldmodel)
		{
			D3D11_Set2D ();
			R2D_ImageColours(0, 0, 0, 1);
			R2D_FillBlock(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height);
			R2D_ImageColours(1, 1, 1, 1);
			return;
		}
	Surf_DrawWorld();
	RQ_RenderBatchClear();

	D3D11_Set2D ();
}

rendererinfo_t d3d11rendererinfo =
{
	"Direct3D11",
	{
		"D3D11",
		"Direct3d11",
		"DirectX11",
		"DX11"
	},
	QR_DIRECT3D11,

	D3D11_Draw_Init,
	D3D11_Draw_Shutdown,

	D3D11_LoadTexture,
	D3D11_LoadTexture8Pal24,
	D3D11_LoadTexture8Pal32,
	D3D11_LoadCompressed,
	D3D11_FindTexture,
	D3D11_AllocNewTexture,
	D3D11_Upload,
	D3D11_DestroyTexture,

	D3D11_R_Init,
	D3D11_R_DeInit,
	D3D11_R_RenderView,

	D3D11_R_NewMap,
	D3D11_R_PreNewMap,

	D3D11_VID_Init,
	D3D11_VID_DeInit,
	D3D11_VID_ApplyGammaRamps,
	D3D11_VID_GetRGBInfo,
	D3D11_VID_SetWindowCaption,

	D3D11_SCR_UpdateScreen,

	D3D11BE_SelectMode,
	D3D11BE_DrawMesh_List,
	D3D11BE_DrawMesh_Single,
	D3D11BE_SubmitBatch,
	D3D11BE_GetTempBatch,
	D3D11BE_DrawWorld,
	D3D11BE_Init,
	D3D11BE_GenBrushModelVBO,
	D3D11BE_ClearVBO,
	D3D11BE_UploadAllLightmaps,
	D3D11BE_SelectEntity,
	D3D11BE_SelectDLight,
	D3D11BE_Scissor,
	D3D11BE_LightCullModel,

	D3D11BE_VBO_Begin,
	D3D11BE_VBO_Data,
	D3D11BE_VBO_Finish,
	D3D11BE_VBO_Destroy,

	"no more"
};
#endif