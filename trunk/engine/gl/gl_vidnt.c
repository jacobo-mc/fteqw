/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"
#include "winquake.h"
#include "resource.h"
#include "shader.h"
#include <commctrl.h>

void STT_Event(void);

#ifndef SetWindowLongPtr	//yes its a define, for unicode support
#define SetWindowLongPtr SetWindowLong
#endif

#ifndef CDS_FULLSCREEN
	#define CDS_FULLSCREEN 4
#endif

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

#ifndef WS_EX_LAYERED
	#define WS_EX_LAYERED 0x00080000
#endif
#ifndef LWA_ALPHA
	#define LWA_ALPHA 0x00000002
#endif
typedef BOOL (WINAPI *lpfnSetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);

extern cvar_t vid_conwidth, vid_conautoscale;


#define WINDOW_CLASS_NAME "FTEGLQuake"

#define MAX_MODE_LIST	128
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

extern cvar_t vid_width;
extern cvar_t vid_height;
extern cvar_t vid_wndalpha;

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

BOOL bSetupPixelFormat(HDC hDC, rendererstate_t *info);

//qboolean VID_SetWindowedMode (int modenum);
//qboolean VID_SetFullDIBMode (int modenum);
qboolean VID_SetWindowedMode (rendererstate_t *info);	//-1 on bpp or hz for default.
qboolean VID_SetFullDIBMode (rendererstate_t *info);	//-1 on bpp or hz for default.

qboolean		scr_skipupdate;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	leavecurrentmode= true;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
extern qboolean	mouseactive;  // from in_win.c
static HICON	hIcon;
static HCURSOR	hArrowCursor, hCustomCursor;
extern qboolean vid_isfullscreen;

unsigned short originalgammaramps[3][256];

qboolean vid_initializing;

qboolean VID_AttachGL (rendererstate_t *info);

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow, dibwindow;

unsigned char	vid_curpal[256*3];

HGLRC	baseRC;
HDC		maindc;


HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

//unsigned short	d_8to16rgbtable[256];
//unsigned	d_8to24rgbtable[256];
//unsigned short	d_8to16bgrtable[256];
//unsigned	d_8to24bgrtable[256];

modestate_t	modestate = MS_UNINIT;

extern float gammapending;


LONG WINAPI GLMainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
qboolean GLAppActivate(BOOL fActive, BOOL minimize);
void ClearAllStates (void);
void VID_UpdateWindowStatus (HWND hWnd);
void GL_Init(void *(*getglfunction) (char *name));

typedef void (APIENTRY *lp3DFXFUNC) (int, int, int, int, int, const void*);
lp3DFXFUNC qglColorTableEXT;
qboolean is8bit = false;
qboolean isPermedia = false;

//====================================
// Note that 0 is MODE_WINDOWED
extern cvar_t	vid_mode;
// Note that 3 is MODE_FULLSCREEN_DEFAULT
extern cvar_t		vid_vsync;
extern cvar_t		_windowed_mouse;
extern cvar_t		vid_hardwaregamma;
extern cvar_t		vid_desktopgamma;
extern cvar_t		gl_lateswap;
extern cvar_t		vid_preservegamma;

extern cvar_t		vid_gl_context_version;
extern cvar_t		vid_gl_context_debug;
extern cvar_t		vid_gl_context_es;
extern cvar_t		vid_gl_context_forwardcompatible;
extern cvar_t		vid_gl_context_compatibility;

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

HMODULE hInstGL = NULL;
HMODULE hInstwgl = NULL;
static qboolean usingminidriver;
static char reqminidriver[MAX_OSPATH];
static char opengldllname[MAX_OSPATH];

#ifdef _DEBUG
//this is a list of the functions that exist in opengles2, as well as wglCreateContextAttribsARB.
//functions not in this list *should* be stubs that just return errors, but we can't always depend on drivers for that... they shouldn't get called.
//this list is just to make it easier to test+debug android gles2 stuff using windows.
static char *gles2funcs[] =
{
#define f(n) #n,
		f(glActiveTexture)
		f(glAttachShader)
		f(glBindAttribLocation)
		f(glBindBuffer)
		f(glBindFramebuffer)
		f(glBindRenderbuffer)
		f(glBindTexture)
		f(glBlendColor)
		f(glBlendEquation)
		f(glBlendEquationSeparate)
		f(glBlendFunc)
		f(glBlendFuncSeparate)
		f(glBufferData)
		f(glBufferSubData)
		f(glCheckFramebufferStatus)
		f(glClear)
		f(glClearColor)
		f(glClearDepthf)
		f(glClearStencil)
		f(glColorMask)
		f(glCompileShader)
		f(glCompressedTexImage2D)
		f(glCompressedTexSubImage2D)
		f(glCopyTexImage2D)
		f(glCopyTexSubImage2D)
		f(glCreateProgram)
		f(glCreateShader)
		f(glCullFace)
		f(glDeleteBuffers)
		f(glDeleteFramebuffers)
		f(glDeleteProgram)
		f(glDeleteRenderbuffers)
		f(glDeleteShader)
		f(glDeleteTextures)
		f(glDepthFunc)
		f(glDepthMask)
		f(glDepthRangef)
		f(glDetachShader)
		f(glDisable)
		f(glDisableVertexAttribArray)
		f(glDrawArrays)
		f(glDrawElements)
		f(glEnable)
		f(glEnableVertexAttribArray)
		f(glFinish)
		f(glFlush)
		f(glFramebufferRenderbuffer)
		f(glFramebufferTexture2D)
		f(glFrontFace)
		f(glGenBuffers)
		f(glGenerateMipmap)
		f(glGenFramebuffers)
 		f(glGenRenderbuffers)
		f(glGenTextures)
		f(glGetActiveAttrib)
		f(glGetActiveUniform)
		f(glGetAttachedShaders)
		f(glGetAttribLocation)
		f(glGetBooleanv)
		f(glGetBufferParameteriv)
		f(glGetError)
		f(glGetFloatv)
		f(glGetFramebufferAttachmentParameteriv)
		f(glGetIntegerv)
		f(glGetProgramiv)
		f(glGetProgramInfoLog)
		f(glGetRenderbufferParameteriv)
		f(glGetShaderiv)
		f(glGetShaderInfoLog)
		f(glGetShaderPrecisionFormat)
		f(glGetShaderSource)
		f(glGetString)
		f(glGetTexParameterfv)
		f(glGetTexParameteriv)
		f(glGetUniformfv)
		f(glGetUniformiv)
		f(glGetUniformLocation)
		f(glGetVertexAttribfv)
		f(glGetVertexAttribiv)
		f(glGetVertexAttribPointerv)
		f(glHint)
		f(glIsBuffer)
		f(glIsEnabled)
		f(glIsFramebuffer)
		f(glIsProgram)
		f(glIsRenderbuffer)
		f(glIsShader)
		f(glIsTexture)
		f(glLineWidth)
		f(glLinkProgram)
		f(glPixelStorei)
		f(glPolygonOffset)
		f(glReadPixels)
		f(glReleaseShaderCompiler)
		f(glRenderbufferStorage)
		f(glSampleCoverage)
		f(glScissor)
		f(glShaderBinary)
		f(glShaderSource)
		f(glStencilFunc)
		f(glStencilFuncSeparate)
		f(glStencilMask)
		f(glStencilMaskSeparate)
		f(glStencilOp)
		f(glStencilOpSeparate)
		f(glTexImage2D)
		f(glTexParameterf)
		f(glTexParameterfv)
		f(glTexParameteri)
		f(glTexParameteriv)
		f(glTexSubImage2D)
		f(glUniform1f)
		f(glUniform1fv)
		f(glUniform1i)
		f(glUniform1iv)
		f(glUniform2f)
		f(glUniform2fv)
		f(glUniform2i)
		f(glUniform2iv)
		f(glUniform3f)
		f(glUniform3fv)
		f(glUniform3i)
		f(glUniform3iv)
		f(glUniform4f)
		f(glUniform4fv)
		f(glUniform4i)
		f(glUniform4iv)
		f(glUniformMatrix2fv)
		f(glUniformMatrix3fv)
		f(glUniformMatrix4fv)
		f(glUseProgram)
		f(glValidateProgram)
		f(glVertexAttrib1f)
		f(glVertexAttrib1fv)
		f(glVertexAttrib2f)
		f(glVertexAttrib2fv)
		f(glVertexAttrib3f)
		f(glVertexAttrib3fv)
		f(glVertexAttrib4f)
		f(glVertexAttrib4fv)
		f(glVertexAttribPointer)
		f(glViewport)
		f(wglCreateContextAttribsARB)
		NULL
};
#endif

//just GetProcAddress with a safty net.
void *getglfunc(char *name)
{
	FARPROC proc;
	proc = qwglGetProcAddress?qwglGetProcAddress(name):NULL;
	if (!proc)
	{
		proc = GetProcAddress(hInstGL, name);
		TRACE(("dbg: getglfunc: gpa %s: success %i\n", name, !!proc));
	}
	else
	{
		TRACE(("dbg: getglfunc: glgpa %s: success %i\n", name, !!proc));
	}

#ifdef _DEBUG
	if (vid_gl_context_es.ival == 2)
	{
		int i;
		for (i = 0; gles2funcs[i]; i++)
		{
			if (!strcmp(name, gles2funcs[i]))
				return proc;
		}
		return NULL;
	}
#endif
	return proc;
}
void *getwglfunc(char *name)
{
	FARPROC proc;
	TRACE(("dbg: getwglfunc: %s: getting\n", name));

	proc = GetProcAddress(hInstGL, name);
	if (!proc)
	{
		if (!hInstwgl)
		{
			TRACE(("dbg: getwglfunc: explicitly loading opengl32.dll\n", name));
			hInstwgl = LoadLibrary("opengl32.dll");
		}
		TRACE(("dbg: getwglfunc: %s: wglgetting\n", name));
		proc = GetProcAddress(hInstwgl, name);
		TRACE(("dbg: getwglfunc: gpa %s: success %i\n", name, !!proc));
		if (!proc)
			Sys_Error("GL function %s was not found in %s\nPossibly you do not have a full enough gl implementation", name, opengldllname);
	}
	TRACE(("dbg: getwglfunc: glgpa %s: success %i\n", name, !!proc));
	return proc;
}

HGLRC (WINAPI *qwglCreateContext)(HDC);
BOOL  (WINAPI *qwglDeleteContext)(HGLRC);
HGLRC (WINAPI *qwglGetCurrentContext)(VOID);
HDC   (WINAPI *qwglGetCurrentDC)(VOID);
PROC  (WINAPI *qwglGetProcAddress)(LPCSTR);
BOOL  (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
BOOL  (WINAPI *qSwapBuffers)(HDC);
int   (WINAPI *qChoosePixelFormat)(HDC, CONST PIXELFORMATDESCRIPTOR *);
BOOL  (WINAPI *qSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
int   (WINAPI *qDescribePixelFormat)(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);

BOOL (WINAPI *qwglSwapIntervalEXT) (int);

BOOL (APIENTRY *qGetDeviceGammaRamp)(HDC hDC, GLvoid *ramp);
BOOL (APIENTRY *qSetDeviceGammaRamp)(HDC hDC, GLvoid *ramp);

BOOL (APIENTRY *qwglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);

HGLRC (APIENTRY *qwglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
#define WGL_CONTEXT_MAJOR_VERSION_ARB		0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB		0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB			0x2093
#define WGL_CONTEXT_FLAGS_ARB				0x2094
#define		WGL_CONTEXT_DEBUG_BIT_ARB					0x0001
#define		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB		0x0002
#define WGL_CONTEXT_PROFILE_MASK_ARB		0x9126
#define		WGL_CONTEXT_CORE_PROFILE_BIT_ARB			0x00000001
#define		WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB	0x00000002
#define		WGL_CONTEXT_ES2_PROFILE_BIT_EXT				0x00000004	/*WGL_CONTEXT_ES2_PROFILE_BIT_EXT*/
#define ERROR_INVALID_VERSION_ARB			0x2095
#define	ERROR_INVALID_PROFILE_ARB		0x2096



qboolean GLInitialise (char *renderer)
{
	if (!hInstGL || strcmp(reqminidriver, renderer))
	{
		usingminidriver = false;
		if (hInstGL)
			FreeLibrary(hInstGL);
		hInstGL=NULL;
		if (hInstwgl)
			FreeLibrary(hInstwgl);
		hInstwgl=NULL;

		Q_strncpyz(reqminidriver, renderer, sizeof(reqminidriver));
		Q_strncpyz(opengldllname, renderer, sizeof(opengldllname));

		if (*renderer && stricmp(renderer, "opengl32.dll") && stricmp(renderer, "opengl32"))
		{
			Con_DPrintf ("Loading renderer dll \"%s\"", renderer);
			hInstGL = LoadLibrary(opengldllname);

			if (hInstGL)
			{
				usingminidriver = true;
				Con_DPrintf (" Success\n");
			}
			else
				Con_DPrintf (" Failed\n");
		}
		else
			hInstGL = NULL;

		if (!hInstGL)
		{
			unsigned int emode;
			strcpy(opengldllname, "opengl32");
			Con_DPrintf ("Loading renderer dll \"%s\"", opengldllname);
			emode = SetErrorMode(SEM_FAILCRITICALERRORS); /*no annoying errors if they use glide*/
			hInstGL = LoadLibrary(opengldllname);
			SetErrorMode(emode);

			if (hInstGL)
				Con_DPrintf (" Success\n");
			else
				Con_DPrintf (" Failed\n");
		}
		if (!hInstGL)
		{
			if (*renderer)
				Con_Printf ("Couldn't load %s or %s\n", renderer, opengldllname);
			else
				Con_Printf ("Couldn't load %s\n", opengldllname);
			return false;
		}
	}
	else
	{
		Con_DPrintf ("Reusing renderer dll %s\n", opengldllname);
	}

	Con_DPrintf ("Loaded renderer dll %s\n", opengldllname);

	// windows dependant
	qwglCreateContext		= (void *)getwglfunc("wglCreateContext");
	qwglDeleteContext		= (void *)getwglfunc("wglDeleteContext");
	qwglGetCurrentContext	= (void *)getwglfunc("wglGetCurrentContext");
	qwglGetCurrentDC		= (void *)getwglfunc("wglGetCurrentDC");
	qwglGetProcAddress		= (void *)getwglfunc("wglGetProcAddress");
	qwglMakeCurrent			= (void *)getwglfunc("wglMakeCurrent");

	if (usingminidriver)
	{
		qSwapBuffers			= (void *)getglfunc("wglSwapBuffers");
		qChoosePixelFormat		= (void *)getglfunc("wglChoosePixelFormat");
		qSetPixelFormat			= (void *)getglfunc("wglSetPixelFormat");
		qDescribePixelFormat	= (void *)getglfunc("wglDescribePixelFormat");
	}
	else
	{
		qSwapBuffers			= SwapBuffers;
		qChoosePixelFormat		= ChoosePixelFormat;
		qSetPixelFormat			= SetPixelFormat;
		qDescribePixelFormat	= DescribePixelFormat;
	}

	qGetDeviceGammaRamp			= (void *)getglfunc("wglGetDeviceGammaRamp3DFX");
	qSetDeviceGammaRamp			= (void *)getglfunc("wglSetDeviceGammaRamp3DFX");

	TRACE(("dbg: GLInitialise: got wgl funcs\n"));

	return true;
}

/*doesn't consider parent offsets*/
RECT centerrect(unsigned int parentleft, unsigned int parenttop, unsigned int parentwidth, unsigned int parentheight, unsigned int cwidth, unsigned int cheight)
{
	RECT r;
	if (modestate!=MS_WINDOWED)
	{
		if (!vid_width.ival)
			cwidth = parentwidth;
		if (!vid_height.ival)
			cheight = parentwidth;
	}

	if (parentwidth < cwidth)
	{
		r.left = parentleft;
		r.right = r.left+parentwidth;
	}
	else
	{
		r.left = parentleft + (parentwidth - cwidth) / 2;
		r.right = r.left + cwidth;
	}

	if (parentheight < cheight)
	{
		r.top = parenttop;
		r.bottom = r.top + parentheight;
	}
	else
	{
		r.top = parenttop + (parentheight - cheight) / 2;
		r.bottom = r.top + cheight;
	}

	return r;
}

void Image_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight);
void *WIN_CreateCursor(char *filename, float hotx, float hoty, float scale)
{
	int width, height;
	BITMAPV5HEADER bi;
	DWORD x,y;
	HCURSOR hAlphaCursor = NULL;
	ICONINFO ii;

	qbyte *rgbadata, *rgbadata_start, *bgradata, *bgradata_start;
	qboolean hasalpha;
	void *filedata;
	int filelen;
	if (!filename || !*filename)
		return NULL;
	filelen = FS_LoadFile(filename, &filedata);
	if (!filedata)
		return NULL;

	rgbadata_start = Read32BitImageFile(filedata, filelen, &width, &height, &hasalpha, "cursor");
	FS_FreeFile(filedata);
	if (!rgbadata_start)
		return NULL;

	if (scale != 1)
	{
		int nw,nh;
		qbyte *nd;
		nw = width * scale;
		nh = height * scale;
		if (nw <= 0 || nh <= 0 || nw > 128 || nh > 128)	//don't go crazy.
			return NULL;
		nd = BZ_Malloc(nw*nh*4);
		Image_ResampleTexture((unsigned int*)rgbadata_start, width, height, (unsigned int*)nd, nw, nh);
		width = nw;
		height = nh;
		BZ_Free(rgbadata_start);
		rgbadata_start = nd;
	}

	memset(&bi,0, sizeof(BITMAPV5HEADER));
	bi.bV5Size			= sizeof(BITMAPV5HEADER);
	bi.bV5Width			= width;
	bi.bV5Height		= height;
	bi.bV5Planes		= 1;
	bi.bV5BitCount		= 32;
	bi.bV5Compression	= BI_BITFIELDS;
	// The following mask specification specifies a supported 32 BPP
	// alpha format for Windows XP.
	//FIXME: can we not just specify it as RGBA? meh.
	bi.bV5RedMask		= 0x00FF0000;
	bi.bV5GreenMask		= 0x0000FF00;
	bi.bV5BlueMask		= 0x000000FF;
	bi.bV5AlphaMask		= 0xFF000000; 

	// Create the DIB section with an alpha channel.
	ii.hbmColor = CreateDIBSection(maindc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&bgradata_start, NULL, 0);

	if (!ii.hbmColor)
	{
		BZ_Free(rgbadata_start);
		return NULL;
	}

	for (rgbadata=rgbadata_start,y=0;y<height;y++)
	{
		bgradata = bgradata_start + (height-1-y)*width*4;
		for (x=0;x<width;x++)
		{
			bgradata[0] = rgbadata[2];
			bgradata[1] = rgbadata[1];
			bgradata[2] = rgbadata[0];
			bgradata[3] = rgbadata[3];
			bgradata+=4;
			rgbadata+=4;
		}
	}

	BZ_Free(rgbadata_start);

	ii.fIcon = FALSE;  // Change fIcon to TRUE to create an alpha icon
	ii.xHotspot = hotx;
	ii.yHotspot = hoty;
	ii.hbmMask = CreateBitmap(width,height,1,1,NULL);

	// Create the alpha cursor with the alpha DIB section.
	hAlphaCursor = CreateIconIndirect(&ii);

	DeleteObject(ii.hbmColor);          
	DeleteObject(ii.hbmMask); 

	return hAlphaCursor;
}

qboolean WIN_SetCursor(void *cursor)
{
	static POINT		current_pos;	//static to avoid bugs in vista(32) with largeaddressaware (this is fixed in win7). fixed exe base address prevents this from going above 2gb.

	hCustomCursor = cursor;

	//move the cursor to ensure the WM_SETCURSOR thing is invoked properly.
	//this ensures all the nastyness of random programs randomly setting the current global cursor is handled by microsoft's code instead of mine.
	//if you're using rawinput there'll be no lost inpuit problems, yay...
	GetCursorPos(&current_pos);
	SetCursorPos(current_pos.x, current_pos.y);
	return true;
}
void WIN_DestroyCursor(void *cursor)
{
	DestroyIcon(cursor);
}

qboolean VID_SetWindowedMode (rendererstate_t *info)
//qboolean VID_SetWindowedMode (int modenum)
{
	int i;
	HDC				hdc;
	int				wwidth, wheight, pleft, ptop, pwidth, pheight;
	RECT			rect;

	modestate = MS_WINDOWED;

	hdc = GetDC(NULL);
	if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
	{
		ReleaseDC(NULL, hdc);
		Con_Printf("Can't run GL in non-RGB mode\n");
		return false;
	}
	ReleaseDC(NULL, hdc);

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = info->width;
	WindowRect.bottom = info->height;


#ifndef FTE_SDL
	if (sys_parentwindow)
	{
		SetWindowLong(sys_parentwindow, GWL_STYLE, GetWindowLong(sys_parentwindow, GWL_STYLE)|WS_OVERLAPPED);
		WindowStyle = WS_CHILDWINDOW|WS_OVERLAPPED;
		ExWindowStyle = 0;

		pleft = sys_parentleft;
		ptop = sys_parenttop;
		pwidth = sys_parentwidth;
		pheight = sys_parentheight;

		WindowRect.right = sys_parentwidth;
		WindowRect.bottom = sys_parentheight;
	}
	else
#endif
	{
		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
					  WS_MINIMIZEBOX;
		ExWindowStyle = 0;

		WindowStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;

		pleft = 0;
		ptop = 0;
		pwidth = GetSystemMetrics(SM_CXSCREEN);
		pheight = GetSystemMetrics(SM_CYSCREEN);

		/*Assume dual monitors, and chop the width to try to put it on only one screen*/
		if (pwidth >= pheight*2)
			pwidth /= 2;
	}

	DIBWidth = WindowRect.right - WindowRect.left;
	DIBHeight = WindowRect.bottom - WindowRect.top;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	wwidth = rect.right - rect.left;
	wheight = rect.bottom - rect.top;

	WindowRect = centerrect(pleft, ptop, pwidth, pheight, wwidth, wheight);

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 WINDOW_CLASS_NAME,
		 FULLENGINENAME,
		 WindowStyle,
		 WindowRect.left, WindowRect.top,
		 WindowRect.right - WindowRect.left,
		 WindowRect.bottom - WindowRect.top,
		 sys_parentwindow,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
	{
		Con_Printf ("Couldn't create DIB window");
		return false;
	}

	SendMessage (dibwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (dibwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	if (!sys_parentwindow)
	{
#ifdef WS_EX_LAYERED
		int av;
		av = 255*vid_wndalpha.value;
		if (av < 70)
			av = 70;
		if (av < 255)
		{
			HMODULE hm = GetModuleHandle("user32.dll");
			lpfnSetLayeredWindowAttributes pSetLayeredWindowAttributes;
			pSetLayeredWindowAttributes = (void*)GetProcAddress(hm, "SetLayeredWindowAttributes");

			if (pSetLayeredWindowAttributes)
			{
				// Set WS_EX_LAYERED on this window
				SetWindowLong(dibwindow, GWL_EXSTYLE, GetWindowLong(dibwindow, GWL_EXSTYLE) | WS_EX_LAYERED);

				// Make this window 70% alpha
				pSetLayeredWindowAttributes(dibwindow, 0, (BYTE)av, LWA_ALPHA);
			}
		}
#endif
	}

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	SetFocus(dibwindow);

//	ShowWindow (dibwindow, SW_SHOWDEFAULT);
//	UpdateWindow (dibwindow);

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.width = Q_atoi(com_argv[i+1]);
	else
	{
		vid.width = 640;
	}

	vid.width &= 0xfff8; // make it a multiple of eight

	if (vid.width < 320)
		vid.width = 320;

	// pick a conheight that matches with correct aspect
	vid.height = vid.width*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.height = Q_atoi(com_argv[i+1]);
	if (vid.height < 200)
		vid.height = 200;

	if (vid.height > info->height)
		vid.height = info->height;
	if (vid.width > info->width)
		vid.width = info->width;

	vid.numpages = 2;

	mainwindow = dibwindow;
	vid_isfullscreen=false;

	CL_UpdateWindowTitle();

	return true;
}

void GLVID_SetCaption(char *text)
{
	SetWindowText(mainwindow, text);
}


qboolean VID_SetFullDIBMode (rendererstate_t *info)
{
	int i;
	HDC				hdc;
	int				wwidth, wheight;
	RECT			rect;

	if (leavecurrentmode)	//don't do this with d3d - d3d should set it's own video mode.
	{	//make windows change res.
		gdevmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
		if (info->bpp)
			gdevmode.dmFields |= DM_BITSPERPEL;
		if (info->rate)
			gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
		gdevmode.dmBitsPerPel = info->bpp;
		if (info->bpp && (gdevmode.dmBitsPerPel < 15))
		{
			Con_Printf("Forcing at least 16bpp\n");
			gdevmode.dmBitsPerPel = 16;
		}
		gdevmode.dmDisplayFrequency = info->rate;
		gdevmode.dmPelsWidth = info->width;
		gdevmode.dmPelsHeight = info->height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			Con_SafePrintf((gdevmode.dmFields&DM_DISPLAYFREQUENCY)?"Windows rejected mode %i*%i*%i*%i\n":"Windows rejected mode %i*%i*%i\n", (int)gdevmode.dmPelsWidth, (int)gdevmode.dmPelsHeight, (int)gdevmode.dmBitsPerPel, (int)gdevmode.dmDisplayFrequency);
			return false;
		}
	}

	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = info->width;
	WindowRect.bottom = info->height;

	DIBWidth = info->width;
	DIBHeight = info->height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	wwidth = rect.right - rect.left;
	wheight = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 WINDOW_CLASS_NAME,
		 FULLENGINENAME,
		 WindowStyle,
		 rect.left, rect.top,
		 wwidth,
		 wheight,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	SendMessage (dibwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (dibwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);


	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.width = Q_atoi(com_argv[i+1]);
	else
		vid.width = 640;

	vid.width &= 0xfff8; // make it a multiple of eight

	if (vid.width < 320)
		vid.width = 320;

	// pick a conheight that matches with correct aspect
	vid.height = vid.width*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.height = Q_atoi(com_argv[i+1]);
	if (vid.height < 200)
		vid.height = 200;

	if (vid.height > info->height)
		vid.height = info->height;
	if (vid.width > info->width)
		vid.width = info->width;

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;
	vid_isfullscreen=true;

	mainwindow = dibwindow;

	return true;
}

extern qboolean gammaworks;
static void ReleaseGL(void);
static void Win_Touch_Init(HWND wnd);
static qboolean CreateMainWindow(rendererstate_t *info)
{
	qboolean		stat;
	if (!info->fullscreen)
	{
		TRACE(("dbg: GLVID_SetMode: VID_SetWindowedMode\n"));
		stat = VID_SetWindowedMode(info);
	}
	else
	{
		TRACE(("dbg: GLVID_SetMode: VID_SetFullDIBMode\n"));
		stat = VID_SetFullDIBMode(info);
	}
	VID_UpdateWindowStatus(mainwindow);

	Win_Touch_Init(mainwindow);

	INS_UpdateGrabs(info->fullscreen, ActiveApp);

	return stat;
}
BOOL CheckForcePixelFormat(rendererstate_t *info);
void VID_UnSetMode (void);
int GLVID_SetMode (rendererstate_t *info, unsigned char *palette)
{
	int				temp;
	qboolean		stat;
#ifndef NPFTE
    MSG				msg;
#endif
//	HDC				hdc;

	TRACE(("dbg: GLVID_SetMode\n"));

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	// Set either the fullscreen or windowed mode
	qwglChoosePixelFormatARB = NULL;
	qwglCreateContextAttribsARB = NULL;
	stat = CreateMainWindow(info);
	if (stat)
	{
		stat = VID_AttachGL(info);
		if (stat)
		{
			TRACE(("dbg: GLVID_SetMode: attaching gl okay\n"));
			if (CheckForcePixelFormat(info))
			{
				HMODULE oldgl = hInstGL;
				hInstGL = NULL;	//don't close the gl library, just in case
				VID_UnSetMode();
				hInstGL = oldgl;

				if (CreateMainWindow(info) && VID_AttachGL(info))
				{
					//we have our multisample window
				}
				else
				{
					//multisample failed
					//try the origional way
					if (!CreateMainWindow(info) || !VID_AttachGL(info))
					{
						Con_Printf("Failed to undo antialising. Giving up.\n");
						return false;	//eek
					}
				}
			}
		}
		else
		{
			TRACE(("dbg: GLVID_SetMode: attaching gl failed\n"));
			return false;
		}
	}

	if (!stat)
	{
		TRACE(("dbg: GLVID_SetMode: VID_Set... failed\n"));
		return false;
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus (mainwindow);
	Cvar_ForceCallback(&vid_conautoscale);

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);

#ifndef NPFTE
	/*I don't like this, but if we */
	while (PeekMessage (&msg, mainwindow, 0, 0, PM_REMOVE))
	{
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}
	Sleep (100);
#endif

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (vid_desktopgamma.value)
	{
		HDC hDC = GetDC(GetDesktopWindow());
		gammaworks = qGetDeviceGammaRamp(hDC, originalgammaramps);
		ReleaseDC(GetDesktopWindow(), hDC);
	}
	else
		gammaworks = qGetDeviceGammaRamp(maindc, originalgammaramps);

	return true;
}

static void ReleaseGL(void)
{
	HGLRC hRC;
   	HDC	  hDC = NULL;

	if (qwglGetCurrentContext)
	{
		hRC = qwglGetCurrentContext();
		hDC = qwglGetCurrentDC();

    	qwglMakeCurrent(NULL, NULL);

    	if (hRC)
    		qwglDeleteContext(hRC);
	}
	qwglGetCurrentContext=NULL;

	if (hDC && dibwindow)
		ReleaseDC(dibwindow, hDC);
}

void VID_UnSetMode (void)
{
	if (mainwindow && vid_initialized)
	{
		GLAppActivate(false, false);

		vid_canalttab = false;
		ReleaseGL();

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);
	}

	if (mainwindow)
	{
		dibwindow=NULL;
	//	ShowWindow(mainwindow, SW_HIDE);
	//	SetWindowLongPtr(mainwindow, GWL_WNDPROC, DefWindowProc);
	//	PostMessage(mainwindow, WM_CLOSE, 0, 0);
		DestroyWindow(mainwindow);
		mainwindow = NULL;
	}

#if 0
	//Logically this code should be active. However...
	//1: vid_restarts are slightly slower if we don't reuse the old dll
	//2: nvidia drivers crash if we shut it down+reload!
	if (hInstGL)
	{
		FreeLibrary(hInstGL);
		hInstGL=NULL;
	}
	if (hInstwgl)
	{
		FreeLibrary(hInstwgl);
		hInstwgl=NULL;
	}
	*opengldllname = 0;
#endif
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (HWND hWnd)
{
	POINT p;
	RECT nr;
	GetClientRect(hWnd, &nr);

	//if its bad then we're probably minimised
	if (nr.right <= nr.left)
		return;
	if (nr.bottom <= nr.top)
		return;

	WindowRect = nr;
	p.x = 0;
	p.y = 0;
	ClientToScreen(hWnd, &p);
	window_x = p.x;
	window_y = p.y;
	window_width = WindowRect.right - WindowRect.left;
	window_height = WindowRect.bottom - WindowRect.top;
	vid.pixelwidth = window_width;
	vid.pixelheight = window_height;

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	INS_UpdateClipCursor ();
}


//====================================

qboolean VID_AttachGL (rendererstate_t *info)
{	//make sure we can get a valid renderer.
	do
	{
		TRACE(("dbg: VID_AttachGL: GLInitialise\n"));
		if (GLInitialise(info->subrenderer))
		{
			maindc = GetDC(mainwindow);
			TRACE(("dbg: VID_AttachGL: bSetupPixelFormat\n"));
			if (bSetupPixelFormat(maindc, info))
				break;
			ReleaseDC(mainwindow, maindc);
		}

		if (!*info->subrenderer || !stricmp(info->subrenderer, "opengl32.dll") || !stricmp(info->subrenderer, "opengl32"))	//go for windows system dir if we failed with the default. Should help to avoid the 3dfx problem.
		{
			char systemgl[MAX_OSPATH+1];
			GetSystemDirectory(systemgl, sizeof(systemgl)-1);
			strncat(systemgl, "\\", sizeof(systemgl)-1);
			if (*info->subrenderer)
				strncat(systemgl, info->subrenderer, sizeof(systemgl)-1);
			else
				strncat(systemgl, "opengl32.dll", sizeof(systemgl)-1);
			TRACE(("dbg: VID_AttachGL: GLInitialise (system dir specific)\n"));
			if (GLInitialise(systemgl))
			{
				maindc = GetDC(mainwindow);
				TRACE(("dbg: VID_AttachGL: bSetupPixelFormat\n"));
				if (bSetupPixelFormat(maindc, info))
					break;
				ReleaseDC(mainwindow, maindc);
			}
		}

		TRACE(("dbg: VID_AttachGL: failed to find a valid dll\n"));
		return false;
	} while(1);

	TRACE(("dbg: VID_AttachGL: qwglCreateContext\n"));

    baseRC = qwglCreateContext(maindc);
	if (!baseRC)
	{
		Con_SafePrintf(CON_ERROR "Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.\n");	//green to make it show.
		return false;
	}
	TRACE(("dbg: VID_AttachGL: qwglMakeCurrent\n"));
    if (!qwglMakeCurrent(maindc, baseRC))
	{
		Con_SafePrintf(CON_ERROR "wglMakeCurrent failed\n");	//green to make it show.
		return false;
	}

	if (developer.ival)
	{
		char *(WINAPI *wglGetExtensionsString)(HDC hdc) = NULL;
		if (!wglGetExtensionsString)
			wglGetExtensionsString = getglfunc("wglGetExtensionsString");
		if (!wglGetExtensionsString)
			wglGetExtensionsString = getglfunc("wglGetExtensionsStringARB");
		if (!wglGetExtensionsString)
			wglGetExtensionsString = getglfunc("wglGetExtensionsStringEXT");
		if (wglGetExtensionsString)
			Con_SafePrintf("WGL extensions: %s\n", wglGetExtensionsString(maindc));
	}

	qwglCreateContextAttribsARB = getglfunc("wglCreateContextAttribsARB");
#if 1//def _DEBUG
	//attempt to promote that to opengl3.
	if (qwglCreateContextAttribsARB)
	{
		HGLRC opengl3;
		int attribs[9];
		char *mv;
		int i = 0;
		char *ver;

		ver = vid_gl_context_version.string;
		if (!*ver && vid_gl_context_es.ival)
			ver = "2.0";

		mv = ver;
		while (*mv)
		{
			if (*mv++ == '.')
				break;
		}

		if (*ver)
		{
			attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
			attribs[i++] = atoi(ver);
		}
		if (*mv)
		{
			attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
			attribs[i++] = atoi(mv);
		}

		//flags
		attribs[i+1] = 0;
		if (vid_gl_context_debug.ival)
			attribs[i+1] |= WGL_CONTEXT_DEBUG_BIT_ARB;
		if (vid_gl_context_forwardcompatible.ival)
			attribs[i+1] |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

		if (attribs[i+1])
		{
			attribs[i] = WGL_CONTEXT_FLAGS_ARB;
			i += 2;
		}

		/*only switch contexts if there's actually a point*/
		if (i || !vid_gl_context_compatibility.ival || vid_gl_context_es.ival)
		{
			attribs[i+1] = 0;
			if (vid_gl_context_es.ival)
				attribs[i+1] |= WGL_CONTEXT_ES2_PROFILE_BIT_EXT;
			else if (vid_gl_context_compatibility.ival)
				attribs[i+1] |= WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
			else
				attribs[i+1] |= WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
			attribs[i] = WGL_CONTEXT_PROFILE_MASK_ARB;
			//WGL_CONTEXT_PROFILE_MASK_ARB is ignored if < 3.2 - however, nvidia do not agree and return errors
			if (atof(ver) >= 3.2 || vid_gl_context_es.ival)
				i+=2;

			attribs[i] = 0;

			if ((opengl3 = qwglCreateContextAttribsARB(maindc, NULL, attribs)))
			{
				qwglMakeCurrent(maindc, NULL);
				qwglDeleteContext(baseRC);

				baseRC = opengl3;
				if (!qwglMakeCurrent( maindc, baseRC ))
				{
					Con_SafePrintf(CON_ERROR "wglMakeCurrent failed\n");	//green to make it show.
					return false;
				}
			}
			else
			{
				DWORD error = GetLastError();
				if (error == (0xc0070000 | ERROR_INVALID_VERSION_ARB))
					Con_Printf("Unsupported OpenGL context version (%s).\n", vid_gl_context_version.string);
				else if (error == (0xc0070000 | ERROR_INVALID_PROFILE_ARB))
					Con_Printf("Unsupported OpenGL profile (%s).\n", vid_gl_context_es.ival?"gles":(vid_gl_context_compatibility.ival?"compat":"core"));
				else if (error == (0xc0070000 | ERROR_INVALID_OPERATION))
					Con_Printf("wglCreateContextAttribsARB returned invalid operation.\n");
				else if (error == (0xc0070000 | ERROR_DC_NOT_FOUND))
					Con_Printf("wglCreateContextAttribsARB returned dc not found.\n");
				else if (error == (0xc0070000 | ERROR_INVALID_PIXEL_FORMAT))
					Con_Printf("wglCreateContextAttribsARB returned dc not found.\n");
				else if (error == (0xc0070000 | ERROR_NO_SYSTEM_RESOURCES))
					Con_Printf("wglCreateContextAttribsARB ran out of system resources.\n");
				else if (error == (0xc0070000 | ERROR_INVALID_PARAMETER))
					Con_Printf("wglCreateContextAttribsARB reported invalid parameter.\n");
				else
					Con_Printf("Unknown error creating an OpenGL (%s) Context.\n", vid_gl_context_version.string);
			}
		}
	}
#endif

	TRACE(("dbg: VID_AttachGL: GL_Init\n"));
	GL_Init(getglfunc);

	if (info->stereo)
	{
		GLboolean ster = false;
		qglGetBooleanv(GL_STEREO, &ster);
		if (!ster)
			Con_Printf("Unable to create stereoscopic/quad-buffered OpenGL context. Please use a different stereoscopic method.\n");
	}

	qwglChoosePixelFormatARB	= getglfunc("wglChoosePixelFormatARB");

	qwglSwapIntervalEXT		= getglfunc("wglSwapIntervalEXT");
	if (qwglSwapIntervalEXT && *vid_vsync.string)
	{
		TRACE(("dbg: VID_AttachGL: qwglSwapIntervalEXT\n"));
		qwglSwapIntervalEXT(vid_vsync.value);
	}
	TRACE(("dbg: VID_AttachGL: qSwapBuffers\n"));
	qglClearColor(0, 0, 0, 1);
	qglClear(GL_COLOR_BUFFER_BIT);
	qSwapBuffers(maindc);

	if (!qGetDeviceGammaRamp) qGetDeviceGammaRamp = (void*)GetDeviceGammaRamp;
	if (!qSetDeviceGammaRamp) qSetDeviceGammaRamp = (void*)SetDeviceGammaRamp;

	return true;
}

void VID_Wait_Override_Callback(struct cvar_s *var, char *oldvalue)
{
	if (qwglSwapIntervalEXT && *vid_vsync.string)
		qwglSwapIntervalEXT(vid_vsync.value);
}

void GLVID_Recenter_f(void)
{
	// 4 unused variables
	//int nw = vid_width.value;
	//int nh = vid_height.value;
	//int nx = 0;
	//int ny = 0;

#ifdef _MSC_VER
#define strtoull _strtoui64
#endif

	if (Cmd_Argc() > 1)
		sys_parentleft = atoi(Cmd_Argv(1));
	if (Cmd_Argc() > 2)
		sys_parenttop = atoi(Cmd_Argv(2));
	if (Cmd_Argc() > 3)
		sys_parentwidth = atoi(Cmd_Argv(3));
	if (Cmd_Argc() > 4)
		sys_parentheight = atoi(Cmd_Argv(4));
	if (Cmd_Argc() > 5)
	{
		HWND newparent = (HWND)(DWORD_PTR)strtoull(Cmd_Argv(5), NULL, 16);
		if (newparent != sys_parentwindow && mainwindow && modestate==MS_WINDOWED)
			SetParent(mainwindow, sys_parentwindow);
		sys_parentwindow = newparent;
	}

	if (sys_parentwindow && modestate==MS_WINDOWED)
	{
		WindowRect = centerrect(sys_parentleft, sys_parenttop, sys_parentwidth, sys_parentheight, sys_parentwidth, sys_parentheight);
		MoveWindow(mainwindow, WindowRect.left, WindowRect.top, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, FALSE);

		VID_UpdateWindowStatus (mainwindow);
		Cvar_ForceCallback(&vid_conautoscale);
	}
}

void VID_WndAlpha_Override_Callback(struct cvar_s *var, char *oldvalue)
{
	//this code tells windows to use the alpha channel of the screen, but does really nasty things with the mouse such that its unplayable.
	//its not useful.
/*	if (modestate==MS_WINDOWED)
	{
		struct qDWM_BLURBEHIND 
		{
			  DWORD dwFlags;
			  BOOL  fEnable;
			  HRGN  hRgnBlur;
			  BOOL  fTransitionOnMaximized;
		} bb = {1, true, NULL, true};
		HRESULT (WINAPI *pDwmEnableBlurBehindWindow)(HWND hWnd,const struct qDWM_BLURBEHIND *pBlurBehind);
		dllfunction_t dwm[] =
		{
			{(void*)&pDwmEnableBlurBehindWindow, "DwmEnableBlurBehindWindow"},
			{NULL,NULL}
		};
		if (Sys_LoadLibrary("dwmapi.dll", dwm))
			pDwmEnableBlurBehindWindow(mainwindow, &bb);
	}
*/

#ifdef WS_EX_LAYERED
	//enable whole-window fixed transparency. should work in win2k+
	//note that this can destroy framerates, and they won't reset when the setting is reverted to 1.
	//be prepared to do a vid_restart.
	if (modestate==MS_WINDOWED)
	{
		int av;
		HMODULE hm = GetModuleHandle("user32.dll");
		lpfnSetLayeredWindowAttributes pSetLayeredWindowAttributes;
		pSetLayeredWindowAttributes = (void*)GetProcAddress(hm, "SetLayeredWindowAttributes");

		av = 255 * var->value;
		if (av < 70)
			av = 70;
		if (av > 255)
			av = 255;

		if (pSetLayeredWindowAttributes)
		{
			// Set WS_EX_LAYERED on this window

			if (av < 255)
			{
				SetWindowLong(mainwindow, GWL_EXSTYLE, GetWindowLong(mainwindow, GWL_EXSTYLE) | WS_EX_LAYERED);

				// Make this window 70% alpha
				pSetLayeredWindowAttributes(mainwindow, 0, (BYTE)av, LWA_ALPHA);
			}
			else
			{
				SetWindowLong(mainwindow, GWL_EXSTYLE, GetWindowLong(mainwindow, GWL_EXSTYLE) & ~WS_EX_LAYERED);
				pSetLayeredWindowAttributes(mainwindow, 0, (BYTE)255, LWA_ALPHA);
			}
		}
	}
#endif
}

void GLVID_SwapBuffers (void)
{
	qSwapBuffers(maindc);

// handle the mouse state when windowed if that's changed

	INS_UpdateGrabs(modestate != MS_WINDOWED, ActiveApp);
}

void OblitterateOldGamma(void)
{
	int i;
	if (vid_preservegamma.value)
		return;

	for (i = 0; i < 256; i++)
	{
		originalgammaramps[0][i] = (i<<8) + i;
		originalgammaramps[1][i] = (i<<8) + i;
		originalgammaramps[2][i] = (i<<8) + i;
	}
}

qboolean GLVID_ApplyGammaRamps (unsigned short *ramps)
{
	if (ramps)
	{
		if (!gammaworks)
			return false;

		if (vid_hardwaregamma.value == 1 && modestate == MS_WINDOWED)
			return false;	//don't do hardware gamma in windowed mode

		if (ActiveApp && vid_hardwaregamma.value)	//this is needed because ATI drivers don't work properly (or when task-switched out).
		{
			if (gammaworks)
			{	//we have hardware gamma applied - if we're doing a BF, we don't want to reset to the default gamma (yuck)
				if (vid_desktopgamma.value)
				{
					HDC hDC = GetDC(GetDesktopWindow());
					qSetDeviceGammaRamp (hDC, ramps);
					ReleaseDC(GetDesktopWindow(), hDC);
				}
				else
				{
					qSetDeviceGammaRamp (maindc, ramps);
				}
			}
			return true;
		}
		return false;
	}
	else
	{
		//revert to default
		if (qSetDeviceGammaRamp)
		{
			OblitterateOldGamma();

			if (vid_desktopgamma.value)
			{
				HDC hDC = GetDC(GetDesktopWindow());
				qSetDeviceGammaRamp (hDC, originalgammaramps);
				ReleaseDC(GetDesktopWindow(), hDC);
			}
			else
			{
				qSetDeviceGammaRamp(maindc, originalgammaramps);
			}
		}
		return true;
	}
}

void GLVID_Crashed(void)
{
	if (qSetDeviceGammaRamp && gammaworks)
	{
		OblitterateOldGamma();
		qSetDeviceGammaRamp(maindc, originalgammaramps);
	}
}

void	GLVID_Shutdown (void)
{
	if (qSetDeviceGammaRamp)
	{
		OblitterateOldGamma();

		if (vid_desktopgamma.value)
		{
			HDC hDC = GetDC(GetDesktopWindow());
			qSetDeviceGammaRamp(hDC, originalgammaramps);
			ReleaseDC(GetDesktopWindow(), hDC);
		}
		else
		{
			qSetDeviceGammaRamp(maindc, originalgammaramps);
		}
	}
	qSetDeviceGammaRamp = NULL;
	qGetDeviceGammaRamp = NULL;

	gammaworks = false;

	GLBE_Shutdown();
	VID_UnSetMode();
}


//==========================================================================

#define 	WGL_DRAW_TO_WINDOW_ARB		0x2001
#define 	WGL_ACCELERATION_ARB		0x2003
#define 	WGL_SUPPORT_OPENGL_ARB		0x2010
#define 	WGL_DOUBLE_BUFFER_ARB		0x2011
#define		WGL_STEREO_ARB				0x2012
#define 	WGL_COLOR_BITS_ARB			0x2014
#define 	WGL_ALPHA_BITS_ARB			0x201B
#define 	WGL_DEPTH_BITS_ARB			0x2022
#define 	WGL_STENCIL_BITS_ARB		0x2023
#define 	WGL_FULL_ACCELERATION_ARB	0x2027
qboolean shouldforcepixelformat;
int forcepixelformat;

BOOL CheckForcePixelFormat(rendererstate_t *info)
{
	if (qwglChoosePixelFormatARB && (info->multisample || info->srgb))
	{
		HDC hDC;
		int valid;
		float fAttribute[] = {0,0};
		UINT numFormats;
		int pixelformat;
		int iAttributes = 0;
		int iAttribute[16*2];
		iAttribute[iAttributes++] = WGL_DRAW_TO_WINDOW_ARB;				iAttribute[iAttributes++] = GL_TRUE;
		iAttribute[iAttributes++] = WGL_SUPPORT_OPENGL_ARB;				iAttribute[iAttributes++] = GL_TRUE;
		iAttribute[iAttributes++] = WGL_ACCELERATION_ARB;				iAttribute[iAttributes++] = WGL_FULL_ACCELERATION_ARB;
		iAttribute[iAttributes++] = WGL_COLOR_BITS_ARB;					iAttribute[iAttributes++] = info->bpp;
		iAttribute[iAttributes++] = WGL_ALPHA_BITS_ARB;					iAttribute[iAttributes++] = 4;
		iAttribute[iAttributes++] = WGL_DEPTH_BITS_ARB;					iAttribute[iAttributes++] = 16;
		iAttribute[iAttributes++] = WGL_STENCIL_BITS_ARB;				iAttribute[iAttributes++] = 8;
		iAttribute[iAttributes++] = WGL_DOUBLE_BUFFER_ARB;				iAttribute[iAttributes++] = GL_TRUE;
		iAttribute[iAttributes++] = WGL_STEREO_ARB;						iAttribute[iAttributes++] = info->stereo;
		if (info->multisample)
		{
			iAttribute[iAttributes++] = WGL_SAMPLE_BUFFERS_ARB;				iAttribute[iAttributes++] = GL_TRUE;
			iAttribute[iAttributes++] = WGL_SAMPLES_ARB,					iAttribute[iAttributes++] = info->multisample;						// Check For 4x Multisampling
		}
		if (info->srgb)
		{
			iAttribute[iAttributes++] = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;	iAttribute[iAttributes++] = GL_TRUE;
		}
		iAttribute[iAttributes++] = 0;									iAttribute[iAttributes++] = 0;


		TRACE(("dbg: bSetupPixelFormat: attempting wglChoosePixelFormatARB (multisample 4)\n"));
		hDC = GetDC(mainwindow);

		valid = qwglChoosePixelFormatARB(hDC,iAttribute,fAttribute,1,&pixelformat,&numFormats);
/*		while ((!valid || numFormats < 1) && iAttribute[19] > 1)
		{	//failed, switch wgl_samples to 2
			iAttribute[19] /= 2;
			TRACE(("dbg: bSetupPixelFormat: attempting wglChoosePixelFormatARB (smaller multisample)\n"));
			valid = qwglChoosePixelFormatARB(hDC,iAttribute,fAttribute,1,&pixelformat,&numFormats);
		}
*/
		ReleaseDC(mainwindow, hDC);
		if (valid && numFormats > 0)
		{
			shouldforcepixelformat = true;
			forcepixelformat = pixelformat;
			return true;
		}
	}
	return false;
}

BYTE IntensityFromShifted(unsigned int index, unsigned int shift, unsigned int bits)
{
	unsigned int val;

	val = (index >> shift) & ((1 << bits) - 1);

	switch (bits)
	{
	case 1:
		val = val ? 0xFF : 0;
		break;
	case 2:
		val |= val << 2;
		val |= val << 4;
		break;
	case 3:
		val = val << (8 - bits);
		val |= val >> 3;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		val = val << (8 - bits);
		val |= val >> bits;
		break;
	case 8:
		break;
	default:
		return 0;
	}

	return val;
}

void FixPaletteInDescriptor(HDC hDC, PIXELFORMATDESCRIPTOR *pfd)
{
	LOGPALETTE *ppal;
	HPALETTE hpal;
	int idx, clrs;

	if (pfd->dwFlags & PFD_NEED_PALETTE)
	{
		clrs = 1 << pfd->cColorBits;

		ppal = Z_Malloc(sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * clrs);

		ppal->palVersion = 0x300;
		ppal->palNumEntries = clrs;

		for (idx = 0; idx < clrs; idx++)
		{
			ppal->palPalEntry[idx].peRed = IntensityFromShifted(idx, pfd->cRedShift, pfd->cRedBits);
			ppal->palPalEntry[idx].peGreen = IntensityFromShifted(idx, pfd->cGreenShift, pfd->cGreenBits);
			ppal->palPalEntry[idx].peBlue = IntensityFromShifted(idx, pfd->cBlueShift, pfd->cBlueBits);
			ppal->palPalEntry[idx].peFlags = 0;
		}

		hpal = CreatePalette(ppal);
		SelectPalette(hDC, hpal, FALSE);
		RealizePalette(hDC);
		Z_Free(ppal);
	}
}

BOOL bSetupPixelFormat(HDC hDC, rendererstate_t *info)
{
    PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW 		// support window
	|  PFD_SUPPORT_OPENGL 	// support OpenGL
	|  PFD_DOUBLEBUFFER,		// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
#ifndef RTLIGHTS
	32,				// 32-bit z-buffer
	0,				// 0 stencil, don't need it unless we're using rtlights
#else
	24,				// 24-bit z-buffer
	8,				// stencil buffer
#endif
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

	TRACE(("dbg: bSetupPixelFormat: ChoosePixelFormat\n"));

	if (info->stereo)
		pfd.dwFlags |= PFD_STEREO;
	if (info->bpp == 15 || info->bpp == 16)
		pfd.cColorBits = 16;

	if (shouldforcepixelformat && qwglChoosePixelFormatARB)	//the extra && is paranoia
	{
		shouldforcepixelformat = false;
		pixelformat = forcepixelformat;
	}
	else
	{
		if ((pixelformat = qChoosePixelFormat(hDC, &pfd)))
		{
			TRACE(("dbg: ChoosePixelFormat 1: worked\n"));

			if (qSetPixelFormat(hDC, pixelformat, &pfd))
			{
				TRACE(("dbg: bSetupPixelFormat: we can use the stencil buffer. woot\n"));
				qDescribePixelFormat(hDC, pixelformat, sizeof(pfd), &pfd);
				FixPaletteInDescriptor(hDC, &pfd);

				if ((pfd.dwFlags & PFD_GENERIC_FORMAT) && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED))
				{
					Con_Printf(CON_WARNING "WARNING: software-rendered opengl context\nPlease install appropriate graphics drivers, or try d3d rendering instead\n");
				}
				else if (pfd.dwFlags & PFD_SWAP_COPY)
					Con_Printf(CON_WARNING "WARNING: buffer swaps will use copy operations\n");
				return TRUE;
			}
		}
		TRACE(("dbg: ChoosePixelFormat 1: no stencil buffer for us\n"));

		pfd.cStencilBits = 0;

		if ( (pixelformat = qChoosePixelFormat(hDC, &pfd)) == 0 )
		{
			Con_Printf("bSetupPixelFormat: ChoosePixelFormat failed (%i)\n", (int)GetLastError());
			return FALSE;
		}
	}

	qDescribePixelFormat(hDC, pixelformat, sizeof(pfd), &pfd);

    if (qSetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        Con_Printf("bSetupPixelFormat: SetPixelFormat failed (%i)\n", (int)GetLastError());
        return FALSE;
    }

	if ((pfd.dwFlags & PFD_GENERIC_FORMAT) && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED))
	{
		Con_Printf(CON_WARNING "WARNING: software-rendered opengl context\nPlease install appropriate graphics drivers, or try d3d rendering instead\n");
	}
	else if (pfd.dwFlags & PFD_SWAP_COPY)
		Con_Printf(CON_WARNING "WARNING: buffer swaps will use copy operations\n");

	FixPaletteInDescriptor(hDC, &pfd);
    return TRUE;
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;

// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (0, i, 0, false);
	}

	Key_ClearStates ();
	INS_ClearStates ();
}

qboolean GLAppActivate(BOOL fActive, BOOL minimize)
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

	ActiveApp = fActive;// && (foregroundwindow==mainwindow);
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
		if (modestate != MS_WINDOWED)
		{
			if (vid_canalttab && vid_wassuspended)
			{
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);

								// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
			}
		}

		gammapending = 0.5;				//delayed gamma force
		Cvar_ForceCallback(&v_gamma);	//so the delay isn't so blatent when you have decent graphics drivers that don't break things.
	}

	if (!fActive)
	{
		if (modestate != MS_WINDOWED)
		{
			if (vid_canalttab)
			{
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}

		Cvar_ForceCallback(&v_gamma);	//wham bam thanks.
	}

	return true;
}

#ifndef TWF_WANTPALM
typedef struct _TOUCHINPUT {
  LONG      x;
  LONG      y;
  HANDLE    hSource;
  DWORD     dwID;
  DWORD     dwFlags;
  DWORD     dwMask;
  DWORD     dwTime;
  ULONG_PTR dwExtraInfo;
  DWORD     cxContact;
  DWORD     cyContact;
} TOUCHINPUT, *PTOUCHINPUT;
DECLARE_HANDLE(HTOUCHINPUT);

#define WM_TOUCH					0x0240 
#define TOUCHINPUTMASKF_CONTACTAREA	0x0004
#define TOUCHEVENTF_DOWN			0x0002
#define TOUCHEVENTF_UP				0x0004
#define TWF_WANTPALM				0x00000002
#endif

static BOOL (WINAPI *pRegisterTouchWindow)(HWND hWnd, ULONG ulFlags);
static BOOL (WINAPI *pGetTouchInputInfo)(HTOUCHINPUT hTouchInput, UINT cInputs, PTOUCHINPUT pInputs, int cbSize);
static BOOL (WINAPI *pCloseTouchInputHandle)(HTOUCHINPUT hTouchInput);
static void Win_Touch_Init(HWND wnd)
{
	HMODULE lib;
	lib = LoadLibrary("user32.dll");
	pRegisterTouchWindow = (void*)GetProcAddress(lib, "RegisterTouchWindow");
	pGetTouchInputInfo = (void*)GetProcAddress(lib, "GetTouchInputInfo");
	pCloseTouchInputHandle = (void*)GetProcAddress(lib, "CloseTouchInputHandle");

	if (pRegisterTouchWindow && pGetTouchInputInfo && pCloseTouchInputHandle)
		pRegisterTouchWindow(wnd, TWF_WANTPALM);
}
static void Win_Touch_Event(int points, HTOUCHINPUT ti)
{
	float sz;
	int i;
	TOUCHINPUT *inputs = malloc(points * sizeof(*inputs)), *input;
	if (inputs)
	{
		if (pGetTouchInputInfo(ti, points, inputs, sizeof(*inputs)))
		{
			for (i = 0, input = inputs; i < points; i++, input++)
			{
				int id = input->dwID+1;	//googling implies the id is generally a low 0-based index. I can't test this. the +1 ensures that mouselook is not broken by someone trying to use a touchscreen at the same time.
				if (input->dwMask & TOUCHINPUTMASKF_CONTACTAREA)
					sz = sqrt((input->cxContact*input->cxContact + input->cyContact*input->cyContact) / 10000.0);
				else
					sz = 0;

				//the web seems to imply that the ids should be low values, <16 or so. hurrah.

				//movement *then* buttons. this should ensure that the cursor is positioned correctly.
				IN_MouseMove(id, true, input->x/100.0f, input->y/100.0f, 0, sz);

				if (input->dwFlags & TOUCHEVENTF_DOWN)
					IN_KeyEvent(id, true, K_MOUSE1, 0);
				if (input->dwFlags & TOUCHEVENTF_UP)
					IN_KeyEvent(id, false, K_MOUSE1, 0);
			}
		}
		free(inputs);
	}

	pCloseTouchInputHandle(ti);
}


/* main window procedure */
LONG WINAPI GLMainWndProc (
	HWND	hWnd,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam)
{
	LONG	lRet = 1;
//	int		fActive, fMinimized;
	int 	temp;
	extern unsigned int uiWheelMessage;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

	switch (uMsg)
	{
		case WM_COPYDATA:
			{
				COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lParam;
				Host_RunFile(cds->lpData, cds->cbData, NULL);
			}
			break;
		case WM_KILLFOCUS:
			GLAppActivate(FALSE, Minimized);
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			ClearAllStates ();
			break;
		case WM_SETFOCUS:
			if (!GLAppActivate(TRUE, Minimized))
				break;
			ClearAllStates ();
			break;

		case WM_TOUCH:
			Win_Touch_Event(LOWORD(wParam), (HTOUCHINPUT)lParam);
			return 0;	//return 0 if we handled it.

		case WM_CREATE:
			break;

		case WM_MOVE:
			VID_UpdateWindowStatus (hWnd);
			Cvar_ForceCallback(&vid_conautoscale);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (!vid_initializing)
				INS_TranslateKeyEvent(wParam, lParam, true, 0);
			break;

//		case WM_UNICHAR:
		case WM_DEADCHAR:
		case WM_SYSDEADCHAR:
		case WM_CHAR:
		case WM_SYSCHAR:
//			if (!vid_initializing)
//				INS_TranslateKeyEvent(wParam, lParam, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (!vid_initializing)
				INS_TranslateKeyEvent(wParam, lParam, false, 0);
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
			{
				temp |= 1;
				if (sys_parentwindow && modestate == MS_WINDOWED)
					SetFocus(hWnd);
			}

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
				if ((short) HIWORD(wParam&0xffffffff) > 0)
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

		case WM_USER:
#ifndef NOMEDIA
			STT_Event();
#endif
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
			vid.isminimized  = (wParam==SIZE_MINIMIZED);
			if (!vid_initializing)
			{
				VID_UpdateWindowStatus (hWnd);
				Cvar_ForceCallback(&vid_conautoscale);
			}
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
//			fActive = LOWORD(wParam);
//			fMinimized = (BOOL) HIWORD(wParam);
//			if (!GLAppActivate(!(fActive == WA_INACTIVE), fMinimized))
				break;//so, urm, tell me microsoft, what changed?
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWNORMAL);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			Cvar_ForceCallback(&vid_conautoscale);

			break;

		case WM_DESTROY:
			if (dibwindow)
				DestroyWindow (dibwindow);
			break;
		case WM_SETCURSOR:
			//only use a custom cursor if the cursor is inside the client area
			switch(lParam&0xffff)
			{
			case 0:
				break;
			case HTCLIENT:
				if (hCustomCursor)	//custom cursor enabled
					SetCursor(hCustomCursor);
				else				//fallback on an arrow cursor, just so we have something visible at startup or so
					SetCursor(hArrowCursor);
				lRet = TRUE;
				break;
			default:
				lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
				break;
			}
			break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}


qboolean GLVID_Is8bit(void) {
	return is8bit;
}


void VID_Init8bitPalette(void)
{
#ifdef GL_USE8BITTEX
#ifdef GL_EXT_paletted_texture
#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB

	// Check for 8bit Extensions and initialize them.
	int i;
	char thePalette[256*3];
	char *oldPalette, *newPalette;

	qglColorTableEXT = (void *)qwglGetProcAddress("glColorTableEXT");
	if (!qglColorTableEXT || !GL_CheckExtension("GL_EXT_shared_texture_palette") || COM_CheckParm("-no8bit"))
		return;

	Con_SafePrintf("8-bit GL extensions enabled.\n");
	qglEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
	oldPalette = (char *) d_8to24rgbtable; //d_8to24table3dfx;
	newPalette = thePalette;
	for (i=0;i<256;i++)
	{
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		oldPalette++;
	}
	qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE,
		(void *) thePalette);
	is8bit = TRUE;

#endif
#endif
}

void GLVID_DeInit (void)
{
	GLVID_Shutdown();
	ActiveApp = false;

	Cvar_Unhook(&vid_vsync);
	Cvar_Unhook(&vid_wndalpha);
	Cmd_RemoveCommand("vid_recenter");

	UnregisterClass(WINDOW_CLASS_NAME, global_hInstance);
}

/*
===================
VID_Init
===================
*/
qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette)
{
//	qbyte	*ptmp;
	DEVMODE	devmode;
	WNDCLASS wc;

	memset(&devmode, 0, sizeof(devmode));

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));
	hArrowCursor = LoadCursor (NULL,IDC_ARROW);

	rf->VID_CreateCursor = WIN_CreateCursor;
	rf->VID_DestroyCursor = WIN_DestroyCursor;
	rf->VID_SetCursor = WIN_SetCursor;

	/* Register the frame class */
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = (WNDPROC)GLMainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = global_hInstance;
    wc.hIcon         = hIcon;
    wc.hCursor       = hArrowCursor;
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass (&wc))	//this isn't really fatal, we'll let the CreateWindow fail instead.
		MessageBox(NULL, "RegisterClass failed", "GAH", 0);

	vid_initialized = false;
	vid_initializing = true;

	if (!GLVID_SetMode (info, palette))
	{
		VID_UnSetMode();
		return false;
	}

	// Check for 3DFX Extensions and initialize them.
	VID_Init8bitPalette();

	vid_canalttab = true;

	Cvar_Hook(&vid_vsync, VID_Wait_Override_Callback);
	Cvar_Hook(&vid_wndalpha, VID_WndAlpha_Override_Callback);

	Cmd_AddCommand("vid_recenter", GLVID_Recenter_f);

	vid_initialized = true;
	vid_initializing = false;

	return true;
}
#endif