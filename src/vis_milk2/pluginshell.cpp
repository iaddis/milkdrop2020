/*
  LICENSE
  -------
Copyright 2005-2013 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Nullsoft nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
    TO DO
    -----
    -done/v1.06:
        -(nothing yet)
        -
        -
    -to do/v1.06:
        -FFT: high freq. data kinda sucks because of the 8-bit samples we get in;
            look for justin to put 16-bit vis data into wa5.
        -make an 'advanced view' button on config panel; hide complicated stuff
            til they click that.
        -put an asterisk(*) next to the 'max framerate' values that
            are ideal (given the current windows display mode or selected FS dispmode).
        -or add checkbox: "smart sync"
            -> matches FPS limit to nearest integer divisor of refresh rate.
        -debug.txt/logging support!
        -audio: make it a DSP plugin? then we could get the complete, continuous waveform
            and overlap our waveform windows, so we'd never miss a brief high note.
        -bugs:
            -vms plugins sometimes freeze after a several-minute pause; I've seen it
                with most of them.  hard to repro, though.
            -running FS on monitor 2, hit ALT-TAB -> minimizes!!!
                -but only if you let go of TAB first.  Let go of ALT first and it's fine!
                -> means it's related to the keyup...
            -fix delayloadhelper leak; one for each launch to config panel/plugin.
            -also, delayload(d3d9.dll) still leaks, if plugin has error initializing and
                quits by returning false from PluginInitialize().
        -add config panel option to ignore fake-fullscreen tips
            -"tip" boxes in dxcontext.cpp
            -"notice" box on WM_ACTIVATEAPP?
        -desktop mode:
            -icon context menus: 'send to', 'cut', and 'copy' links do nothing.
                -http://netez.com/2xExplorer/shellFAQ/bas_context.html
            -create a 2nd texture to render all icon text labels into
                (they're the sole reason that desktop mode is slow)
            -in UpdateIconBitmaps, don't read the whole bitmap and THEN
                realize it's a dupe; try to compare icon filename+index or somethign?
            -DRAG AND DROP.  COMPLICATED; MANY DETAILS.
                -http://netez.com/2xExplorer/shellFAQ/adv_drag.html
                -http://www.codeproject.com/shell/explorerdragdrop.asp
                -hmm... you can't drag icons between the 2 desktops (ugh)
            -multiple delete/open/props/etc
            -delete + enter + arrow keys.
            -try to solve mysteries w/ShellExecuteEx() and desktop *shortcuts* (*.lnk).
            -(notice that when icons are selected, they get modulated by the
                highlight color, when they should be blended 50% with that color.)

    ---------------------------
    final touches:
        -Tests:
            -make sure desktop still functions/responds properly when winamp paused
            -desktop mode + multimon:
                -try desktop mode on all monitors
                -try moving taskbar around; make sure icons are in the
                    right place, that context menus (general & for
                    specific icons) pop up in the right place, and that
                    text-off-left-edge is ok.
                -try setting the 2 monitors to different/same resolutions
        -check tab order of config panel controls!
        -Clean All
        -build in release mode to include in the ZIP
        -leave only one file open in workspace: README.TXT.
        -TEMPORARILY "ATTRIB -R" ALL FILES BEFORE ZIPPING THEM!

    ---------------------------
    KEEP IN VIEW:
        -EMBEDWND:
            -kiv: on resize of embedwnd, it's out of our control; winamp
                resizes the child every time the mouse position changes,
                and we have to cleanup & reallocate everything, b/c we
                can't tell when the resize begins & ends.
                [justin said he'd fix in wa5, though]
            -kiv: with embedded windows of any type (plugin, playlist, etc.)
                you can't place the winamp main wnd over them.
            -kiv: embedded windows are child windows and don't get the
                WM_SETFOCUS or WM_KILLFOCUS messages when they get or lose
                the focus.  (For a workaround, see milkdrop & scroll lock key.)
            -kiv: tiny bug (IGNORE): when switching between embedwnd &
                no-embedding, the window gets scooted a tiny tiny bit.
        -kiv: fake fullscreen mode w/multiple monitors: there is no way
            to keep the taskbar from popping up [potentially overtop of
            the plugin] when you click on something besides the plugin.
            To get around this, use true fullscreen mode.
        -kiv: max_fps implementation assumptions:
            -that most computers support high-precision timer
            -that no computers [regularly] sleep for more than 1-2 ms
                when you call Sleep(1) after timeBeginPeriod(1).
        -reminder: if vms_desktop.dll's interface needs changed,
            it will have to be renamed!  (version # upgrades are ok
            as long as it won't break on an old version; if the
            new functionality is essential, rename the DLL.)

    ---------------------------
    REMEMBER:
        -GF2MX + GF4 have icon scooting probs in desktop mode
            (when taskbar is on upper or left edge of screen)
        -Radeon is the one w/super slow text probs @ 1280x1024.
            (it goes unstable after you show playlist AND helpscr; -> ~1 fps)
        -Mark's win98 machine has hidden cursor (in all modes),
            but no one else seems to have this problem.
        -links:
            -win2k-only-style desktop mode: (uses VirtualAllocEx, vs. DLL Injection)
                http://www.digiwar.com/scripts/renderpage.php?section=2&subsection=2
            -http://www.experts-exchange.com/Programming/Programming_Platforms/Win_Prog/Q_20096218.html
*/

#include "api.h"
#include "pluginshell.h"
#include "utility.h"
#include "defines.h"
#include "shell_defines.h"
#include "resource.h"
#include "vis.h"
#include <multimon.h>
#include "wa_ipc.h"
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")    // for timeGetTime
#pragma comment(lib,"Shlwapi.lib")    // for timeGetTime
	
// STATE VALUES & VERTEX FORMATS FOR HELP SCREEN TEXTURE:
#define TEXT_SURFACE_NOT_READY  0
#define TEXT_SURFACE_REQUESTED  1
#define TEXT_SURFACE_READY      2
#define TEXT_SURFACE_ERROR      3

extern winampVisModule mod1;


CPluginShell::CPluginShell()
{
	// this should remain empty!
}

CPluginShell::~CPluginShell()
{
	// this should remain empty!
}

eScrMode  CPluginShell::GetScreenMode()
{
	return m_screenmode;
};
int       CPluginShell::GetFrame()
{
	return m_frame;
};
float     CPluginShell::GetTime()
{
	return m_time;
};
float     CPluginShell::GetFps()
{
	return m_fps;
};
HWND      CPluginShell::GetPluginWindow()
{
	if (m_lpDX) return m_lpDX->GetHwnd();       else return NULL;
};
int       CPluginShell::GetWidth()
{
	if (m_lpDX) return m_lpDX->m_client_width;  else return 0;
};
int       CPluginShell::GetHeight()
{
	if (m_lpDX) return m_lpDX->m_client_height; else return 0;
};
HWND      CPluginShell::GetWinampWindow()
{
	return m_hWndWinamp;
};
HINSTANCE CPluginShell::GetInstance()
{
	return m_hInstance;
};
wchar_t* CPluginShell::GetPluginsDirPath()
{
	return m_szPluginsDirPath;
};
wchar_t* CPluginShell::GetConfigIniFile()
{
	return m_szConfigIniFile;
};
char* CPluginShell::GetConfigIniFileA()
{
	return m_szConfigIniFileA;
}
int       CPluginShell::GetBitDepth()
{
	return m_lpDX->GetBitDepth();
};
LPDIRECT3DDEVICE9 CPluginShell::GetDevice()
{
	if (m_lpDX) return m_lpDX->m_lpDevice; else return NULL;
};
D3DCAPS9* CPluginShell::GetCaps()
{
	if (m_lpDX) return &(m_lpDX->m_caps);  else return NULL;
};
D3DFORMAT CPluginShell::GetBackBufFormat()
{
	if (m_lpDX) return m_lpDX->m_current_mode.display_mode.Format; else return D3DFMT_UNKNOWN;
};
D3DFORMAT CPluginShell::GetBackBufZFormat()
{
	if (m_lpDX) return m_lpDX->GetZFormat(); else return D3DFMT_UNKNOWN;
};
char* CPluginShell::GetDriverFilename()
{
	if (m_lpDX) return m_lpDX->GetDriver(); else return NULL;
};
char* CPluginShell::GetDriverDescription()
{
	if (m_lpDX) return m_lpDX->GetDesc(); else return NULL;
};

int CPluginShell::InitNondx9Stuff()
{
	timeBeginPeriod(1);
	m_fftobj.Init(576, NUM_FREQUENCIES);
	return AllocateMyNonDx9Stuff();
}

void CPluginShell::CleanUpNondx9Stuff()
{
	timeEndPeriod(1);
	CleanUpMyNonDx9Stuff();
	m_fftobj.CleanUp();
}

int CPluginShell::AllocateDX9Stuff()
{
	int ret = AllocateMyDX9Stuff();
	return ret;
}

void CPluginShell::CleanUpDX9Stuff(int final_cleanup)
{
	// ALWAYS unbind the textures before releasing textures,
	// otherwise they might still have a hanging reference!
	if (m_lpDX && m_lpDX->m_lpDevice)
	{
		for (int i=0; i<16; i++)
			m_lpDX->m_lpDevice->SetTexture(i, NULL);
	}

	CleanUpMyDX9Stuff(final_cleanup);
}
void CPluginShell::OnUserResizeWindow()
{
	// Update window properties
	RECT w, c;
	GetWindowRect(m_lpDX->GetHwnd(), &w);
	GetClientRect(m_lpDX->GetHwnd(), &c);

	WINDOWPLACEMENT wp;
	ZeroMemory(&wp, sizeof(wp));
	wp.length = sizeof(wp);
	GetWindowPlacement(m_lpDX->GetHwnd(), &wp);

	// convert client rect from client coords to screen coords:
	// (window rect is already in screen coords...)
	POINT p;
	p.x = c.left;
	p.y = c.top;
	if (ClientToScreen(m_lpDX->GetHwnd(), &p))
	{
		c.left += p.x;
		c.right += p.x;
		c.top += p.y;
		c.bottom += p.y;
	}

	if (wp.showCmd != SW_SHOWMINIMIZED)
	{
		int new_REAL_client_w = c.right-c.left;
		int new_REAL_client_h = c.bottom-c.top;

		// kiv: could we just resize when the *snapped* w/h changes?  slightly more ideal...
		if (m_lpDX->m_REAL_client_width  != new_REAL_client_w ||
		    m_lpDX->m_REAL_client_height != new_REAL_client_h)
		{
			CleanUpDX9Stuff(0);
			if (!m_lpDX->OnUserResizeWindow(&w, &c))
			{
				return;
			}
			if (!AllocateDX9Stuff())
			{
				m_lpDX->m_ready = false;   // flag to exit
				return;
			}
		}

		// save the new window position:
		if (wp.showCmd==SW_SHOWNORMAL)
			m_lpDX->SaveWindow();
	}
}

void CPluginShell::StuffParams(DXCONTEXT_PARAMS *pParams)
{
	pParams->screenmode   = m_screenmode;
	pParams->display_mode = m_disp_mode_fs;
	pParams->nbackbuf     = 1;
	pParams->m_dualhead_horz = m_dualhead_horz;
	pParams->m_dualhead_vert = m_dualhead_vert;
	pParams->m_skin = (m_screenmode==WINDOWED) ? m_skin : 0;
	switch (m_screenmode)
	{
	case WINDOWED:
		pParams->allow_page_tearing = m_allow_page_tearing_w;
		pParams->adapter_guid       = m_adapter_guid_windowed;
		pParams->multisamp          = m_multisample_windowed;
		strcpy(pParams->adapter_devicename, m_adapter_devicename_windowed);
		break;
	case FULLSCREEN:
	case FAKE_FULLSCREEN:
		pParams->allow_page_tearing = m_allow_page_tearing_fs;
		pParams->adapter_guid       = m_adapter_guid_fullscreen;
		pParams->multisamp          = m_multisample_fullscreen;
		strcpy(pParams->adapter_devicename, m_adapter_devicename_fullscreen);
		break;
	}
	pParams->parent_window = NULL;
}

#define IPC_IS_PLAYING_VIDEO 501 // from wa_ipc.h
#define IPC_SET_VIS_FS_FLAG 631 // a vis should send this message with 1/as param to notify winamp that it has gone to or has come back from fullscreen mode

void CPluginShell::ToggleFullScreen()
{
	CleanUpDX9Stuff(0);

	switch (m_screenmode)
	{
	case WINDOWED:
		m_screenmode = m_fake_fullscreen_mode ? FAKE_FULLSCREEN : FULLSCREEN;
		if (m_screenmode == FULLSCREEN && SendMessage(GetWinampWindow(), WM_WA_IPC, 0, IPC_IS_PLAYING_VIDEO) > 1)
		{
			m_screenmode = FAKE_FULLSCREEN;
		}
		SendMessage(GetWinampWindow(), WM_WA_IPC, 1, IPC_SET_VIS_FS_FLAG);
		break;
	case FULLSCREEN:
	case FAKE_FULLSCREEN:
		m_screenmode = WINDOWED;
		SendMessage(GetWinampWindow(), WM_WA_IPC, 0, IPC_SET_VIS_FS_FLAG);
		break;
	}

	DXCONTEXT_PARAMS params;
	StuffParams(&params);

	if (!m_lpDX->StartOrRestartDevice(&params))
	{
		return;
	}

	if (!AllocateDX9Stuff())
	{
		m_lpDX->m_ready = false;   // flag to exit
		return;
	}

	SetForegroundWindow(m_lpDX->GetHwnd());
	SetActiveWindow(m_lpDX->GetHwnd());
	SetFocus(m_lpDX->GetHwnd());
}

int CPluginShell::InitDirectX()
{
	m_lpDX = new DXContext(m_hWndWinamp,m_hInstance,CLASSNAME,WINDOWCAPTION,CPluginShell::WindowProc,(LONG_PTR)this, m_minimize_winamp, m_szConfigIniFile);

	if (!m_lpDX)
	{
		wchar_t title[64];
		MessageBoxW(NULL, WASABI_API_LNGSTRINGW(IDS_UNABLE_TO_INIT_DXCONTEXT),
				    WASABI_API_LNGSTRINGW_BUF(IDS_MILKDROP_ERROR, title, 64),
				    MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
		return FALSE;
	}

	if (m_lpDX->m_lastErr != S_OK)
	{
		// warning messagebox will have already been given
		delete m_lpDX;
		return FALSE;
	}

	// initialize graphics
	DXCONTEXT_PARAMS params;
	StuffParams(&params);

	if (!m_lpDX->StartOrRestartDevice(&params))
	{
		delete m_lpDX;
		m_lpDX = NULL;
		return FALSE;
	}

	return TRUE;
}

void CPluginShell::CleanUpDirectX()
{
	SafeDelete(m_lpDX);
}

int CPluginShell::PluginPreInitialize(HWND hWinampWnd, HINSTANCE hWinampInstance)
{
	// PROTECTED CONFIG PANEL SETTINGS (also see 'private' settings, below)
	m_fake_fullscreen_mode  = 0;
	m_max_fps_fs            = 30;
	m_max_fps_dm            = 30;
	m_max_fps_w             = 30;
	m_show_press_f1_msg     = 1;
	m_allow_page_tearing_w  = 1;
	m_allow_page_tearing_fs = 0;
	m_allow_page_tearing_dm = 0;
	m_minimize_winamp       = 1;
	m_dualhead_horz         = 2;
	m_dualhead_vert         = 1;
	m_save_cpu              = 1;
	m_skin                  = 1;
	m_fix_slow_text         = 0;

	m_disp_mode_fs.Width = DEFAULT_FULLSCREEN_WIDTH;
	m_disp_mode_fs.Height = DEFAULT_FULLSCREEN_HEIGHT;
	m_disp_mode_fs.Format = D3DFMT_UNKNOWN;
	m_disp_mode_fs.RefreshRate = 60;
	// better yet - in case there is no config INI file saved yet, use the current display mode (if detectable) as the default fullscreen res:
	DEVMODE dm;
	dm.dmSize = sizeof(dm);
	dm.dmDriverExtra = 0;
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm))
	{
		m_disp_mode_fs.Width  = dm.dmPelsWidth;
		m_disp_mode_fs.Height = dm.dmPelsHeight;
		m_disp_mode_fs.RefreshRate = dm.dmDisplayFrequency;
		m_disp_mode_fs.Format = (dm.dmBitsPerPel==16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
	}

	// PROTECTED STRUCTURES/POINTERS
	ZeroMemory(&m_sound, sizeof(td_soundinfo));
	for (int ch=0; ch<2; ch++)
		for (int i=0; i<3; i++)
		{
			m_sound.infinite_avg[ch][i] = m_sound.avg[ch][i] = m_sound.med_avg[ch][i] = m_sound.long_avg[ch][i] = 1.0f;
		}

	// GENERAL PRIVATE STUFF
	//m_screenmode: set at end (derived setting)
	m_frame = 0;
	m_time = 0;
	m_fps = 30;
	m_hWndWinamp = hWinampWnd;
	m_hInstance = hWinampInstance;
	m_lpDX = NULL;
	m_szPluginsDirPath[0] = 0;  // will be set further down
	m_szConfigIniFile[0] = 0;  // will be set further down
	// m_szPluginsDirPath:

	{
		// get path to INI file & read in prefs/settings right away, so DumpMsg works!
		GetModuleFileNameW(m_hInstance, m_szPluginsDirPath, MAX_PATH);
		wchar_t *p = m_szPluginsDirPath + wcslen(m_szPluginsDirPath);
		while (p >= m_szPluginsDirPath && *p != L'\\') p--;
		if (++p >= m_szPluginsDirPath) *p = 0;
	}

	swprintf(m_szConfigIniFile, L"%s%s", m_szPluginsDirPath, INIFILE);
	WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, m_szConfigIniFile, -1, m_szConfigIniFileA, sizeof(m_szConfigIniFileA), NULL, NULL);

	// PRIVATE CONFIG PANEL SETTINGS
	m_multisample_fullscreen = D3DMULTISAMPLE_NONE;
	m_multisample_desktop = D3DMULTISAMPLE_NONE;
	m_multisample_windowed = D3DMULTISAMPLE_NONE;
	ZeroMemory(&m_adapter_guid_fullscreen, sizeof(GUID));
	ZeroMemory(&m_adapter_guid_desktop , sizeof(GUID));
	ZeroMemory(&m_adapter_guid_windowed , sizeof(GUID));
	m_adapter_devicename_windowed[0]   = 0;
	m_adapter_devicename_fullscreen[0] = 0;
	m_adapter_devicename_desktop[0]    = 0;


	// PRIVATE RUNTIME SETTINGS
	m_lost_focus = 0;
	m_hidden     = 0;
	m_resizing   = 0;
	m_exiting    = 0;

	// PRIVATE - MORE TIMEKEEPING
	m_last_raw_time = 0;
	memset(m_time_hist, 0, sizeof(m_time_hist));
	m_time_hist_pos = 0;
	if (!QueryPerformanceFrequency(&m_high_perf_timer_freq))
		m_high_perf_timer_freq.QuadPart = 0;
	m_prev_end_of_frame.QuadPart = 0;

	// PRIVATE AUDIO PROCESSING DATA
	//(m_fftobj needs no init)
	memset(m_oldwave[0], 0, sizeof(float)*576);
	memset(m_oldwave[1], 0, sizeof(float)*576);
	m_prev_align_offset[0] = 0;
	m_prev_align_offset[1] = 0;
	m_align_weights_ready = 0;

	//-----

	m_screenmode = NOT_YET_KNOWN;

	OverrideDefaults();
	ReadConfig();

	m_screenmode = WINDOWED;

	MyPreInitialize();
	MyReadConfig();

	//-----

	return TRUE;
}

int CPluginShell::PluginInitialize()
{
	// note: initialize GDI before DirectX.  Also separate them because
	// when we change windowed<->fullscreen, or lose the device and restore it,
	// we don't want to mess with any (persistent) GDI stuff.

	if (!InitDirectX())        return FALSE;  // gives its own error messages
	if (!InitNondx9Stuff())    return FALSE;  // gives its own error messages
	if (!AllocateDX9Stuff())   return FALSE;  // gives its own error messages

	return TRUE;
}

void CPluginShell::PluginQuit()
{
	CleanUpDX9Stuff(1);
	CleanUpNondx9Stuff();
	CleanUpDirectX();

	SetFocus(m_hWndWinamp);
	SetActiveWindow(m_hWndWinamp);
	SetForegroundWindow(m_hWndWinamp);
}

void CPluginShell::ReadConfig()
{
	int old_ver    = GetPrivateProfileIntW(L"settings",L"version"   ,-1,m_szConfigIniFile);
	int old_subver = GetPrivateProfileIntW(L"settings",L"subversion",-1,m_szConfigIniFile);

	// nuke old settings from prev. version:
	if (old_ver < INT_VERSION)
		return;
	else if (old_subver < INT_SUBVERSION)
		return;

	//D3DMULTISAMPLE_TYPE m_multisample_fullscreen;
	//D3DMULTISAMPLE_TYPE m_multisample_desktop;
	//D3DMULTISAMPLE_TYPE m_multisample_windowed;
	m_multisample_fullscreen      = (D3DMULTISAMPLE_TYPE)GetPrivateProfileIntW(L"settings",L"multisample_fullscreen",m_multisample_fullscreen,m_szConfigIniFile);
	m_multisample_desktop         = (D3DMULTISAMPLE_TYPE)GetPrivateProfileIntW(L"settings",L"multisample_desktop",m_multisample_desktop,m_szConfigIniFile);
	m_multisample_windowed        = (D3DMULTISAMPLE_TYPE)GetPrivateProfileIntW(L"settings",L"multisample_windowed"  ,m_multisample_windowed  ,m_szConfigIniFile);

	//GUID m_adapter_guid_fullscreen
	//GUID m_adapter_guid_desktop
	//GUID m_adapter_guid_windowed
	char str[256];
	GetPrivateProfileString("settings","adapter_guid_fullscreen","",str,sizeof(str)-1,m_szConfigIniFileA);
	TextToGuid(str, &m_adapter_guid_fullscreen);
	GetPrivateProfileString("settings","adapter_guid_desktop","",str,sizeof(str)-1,m_szConfigIniFileA);
	TextToGuid(str, &m_adapter_guid_desktop);
	GetPrivateProfileString("settings","adapter_guid_windowed","",str,sizeof(str)-1,m_szConfigIniFileA);
	TextToGuid(str, &m_adapter_guid_windowed);
	GetPrivateProfileString("settings","adapter_devicename_fullscreen","",m_adapter_devicename_fullscreen,sizeof(m_adapter_devicename_fullscreen)-1,m_szConfigIniFileA);
	GetPrivateProfileString("settings","adapter_devicename_desktop",   "",m_adapter_devicename_desktop   ,sizeof(m_adapter_devicename_desktop)-1,m_szConfigIniFileA);
	GetPrivateProfileString("settings","adapter_devicename_windowed",  "",m_adapter_devicename_windowed  ,sizeof(m_adapter_devicename_windowed)-1,m_szConfigIniFileA);

	m_fake_fullscreen_mode = GetPrivateProfileIntW(L"settings",L"fake_fullscreen_mode",m_fake_fullscreen_mode,m_szConfigIniFile);
	m_max_fps_fs           = GetPrivateProfileIntW(L"settings",L"max_fps_fs",m_max_fps_fs,m_szConfigIniFile);
	m_max_fps_dm           = GetPrivateProfileIntW(L"settings",L"max_fps_dm",m_max_fps_dm,m_szConfigIniFile);
	m_max_fps_w            = GetPrivateProfileIntW(L"settings",L"max_fps_w" ,m_max_fps_w ,m_szConfigIniFile);
	m_allow_page_tearing_w = GetPrivateProfileIntW(L"settings",L"allow_page_tearing_w",m_allow_page_tearing_w,m_szConfigIniFile);
	m_allow_page_tearing_fs= GetPrivateProfileIntW(L"settings",L"allow_page_tearing_fs",m_allow_page_tearing_fs,m_szConfigIniFile);
	m_allow_page_tearing_dm= GetPrivateProfileIntW(L"settings",L"allow_page_tearing_dm",m_allow_page_tearing_dm,m_szConfigIniFile);
	m_minimize_winamp      = GetPrivateProfileIntW(L"settings",L"minimize_winamp",m_minimize_winamp,m_szConfigIniFile);
	m_dualhead_horz        = GetPrivateProfileIntW(L"settings",L"dualhead_horz",m_dualhead_horz,m_szConfigIniFile);
	m_dualhead_vert        = GetPrivateProfileIntW(L"settings",L"dualhead_vert",m_dualhead_vert,m_szConfigIniFile);
	m_save_cpu             = GetPrivateProfileIntW(L"settings",L"save_cpu",m_save_cpu,m_szConfigIniFile);
	m_skin                 = GetPrivateProfileIntW(L"settings",L"skin",m_skin,m_szConfigIniFile);
	m_fix_slow_text        = GetPrivateProfileIntW(L"settings",L"fix_slow_text",m_fix_slow_text,m_szConfigIniFile);

	//D3DDISPLAYMODE m_fs_disp_mode
	m_disp_mode_fs.Width           = GetPrivateProfileIntW(L"settings",L"disp_mode_fs_w", m_disp_mode_fs.Width           ,m_szConfigIniFile);
	m_disp_mode_fs.Height           = GetPrivateProfileIntW(L"settings",L"disp_mode_fs_h",m_disp_mode_fs.Height          ,m_szConfigIniFile);
	m_disp_mode_fs.RefreshRate = GetPrivateProfileIntW(L"settings",L"disp_mode_fs_r",m_disp_mode_fs.RefreshRate,m_szConfigIniFile);
	m_disp_mode_fs.Format      = (D3DFORMAT)GetPrivateProfileIntW(L"settings",L"disp_mode_fs_f",m_disp_mode_fs.Format     ,m_szConfigIniFile);

	// note: we don't call MyReadConfig() yet, because we
	// want to completely finish CPluginShell's preinit (and ReadConfig)
	// before calling CPlugin's preinit and ReadConfig.
}

void CPluginShell::WriteConfig()
{
	//D3DMULTISAMPLE_TYPE m_multisample_fullscreen;
	//D3DMULTISAMPLE_TYPE m_multisample_desktop;
	//D3DMULTISAMPLE_TYPE m_multisample_windowed;
	WritePrivateProfileIntW((int)m_multisample_fullscreen,L"multisample_fullscreen",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW((int)m_multisample_desktop   ,L"multisample_desktop"   ,m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW((int)m_multisample_windowed  ,L"multisample_windowed"  ,m_szConfigIniFile,L"settings");

	//GUID m_adapter_guid_fullscreen
	//GUID m_adapter_guid_desktop
	//GUID m_adapter_guid_windowed
	char str[256];
	GuidToText(&m_adapter_guid_fullscreen, str, sizeof(str));
	WritePrivateProfileString("settings","adapter_guid_fullscreen",str,m_szConfigIniFileA);
	GuidToText(&m_adapter_guid_desktop, str, sizeof(str));
	WritePrivateProfileString("settings","adapter_guid_desktop",str,m_szConfigIniFileA);
	GuidToText(&m_adapter_guid_windowed,   str, sizeof(str));
	WritePrivateProfileString("settings","adapter_guid_windowed"  ,str,m_szConfigIniFileA);
	WritePrivateProfileString("settings","adapter_devicename_fullscreen",m_adapter_devicename_fullscreen,m_szConfigIniFileA);
	WritePrivateProfileString("settings","adapter_devicename_desktop"   ,m_adapter_devicename_desktop   ,m_szConfigIniFileA);
	WritePrivateProfileString("settings","adapter_devicename_windowed"  ,m_adapter_devicename_windowed  ,m_szConfigIniFileA);

	WritePrivateProfileIntW(m_fake_fullscreen_mode,L"fake_fullscreen_mode",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_max_fps_fs,L"max_fps_fs",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_max_fps_dm,L"max_fps_dm",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_max_fps_w ,L"max_fps_w" ,m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_show_press_f1_msg,L"show_press_f1_msg",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_allow_page_tearing_w,L"allow_page_tearing_w",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_allow_page_tearing_fs,L"allow_page_tearing_fs",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_allow_page_tearing_dm,L"allow_page_tearing_dm",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_minimize_winamp,L"minimize_winamp",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_dualhead_horz,L"dualhead_horz",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_dualhead_vert,L"dualhead_vert",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_save_cpu,L"save_cpu",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_skin,L"skin",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_fix_slow_text,L"fix_slow_text",m_szConfigIniFile,L"settings");

	//D3DDISPLAYMODE m_fs_disp_mode
	WritePrivateProfileIntW(m_disp_mode_fs.Width          ,L"disp_mode_fs_w",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_disp_mode_fs.Height          ,L"disp_mode_fs_h",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_disp_mode_fs.RefreshRate,L"disp_mode_fs_r",m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(m_disp_mode_fs.Format     ,L"disp_mode_fs_f",m_szConfigIniFile,L"settings");

	WritePrivateProfileIntW(INT_VERSION            ,L"version"    ,m_szConfigIniFile,L"settings");
	WritePrivateProfileIntW(INT_SUBVERSION         ,L"subversion" ,m_szConfigIniFile,L"settings");

	// finally, save the plugin's unique settings:
	MyWriteConfig();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

int CPluginShell::PluginRender(unsigned char *pWaveL, unsigned char *pWaveR)//, unsigned char *pSpecL, unsigned char *pSpecR)
{
	// return FALSE here to tell Winamp to terminate the plugin

	if (!m_lpDX || !m_lpDX->m_ready)
	{
		// note: 'm_ready' will go false when a device reset fatally fails
		//       (for example, when user resizes window, or toggles fullscreen.)
		m_exiting = 1;
		return false;   // EXIT THE PLUGIN
	}

	m_lost_focus = (GetFocus() != GetPluginWindow());

	if ((m_screenmode==WINDOWED   && m_hidden) ||
	    (m_screenmode==FULLSCREEN && m_lost_focus) ||
	    (m_screenmode==WINDOWED   && m_resizing)
	   )
	{
		Sleep(30);
		return true;
	}

	// test for lost device
	// (this happens when device is fullscreen & user alt-tabs away,
	//  or when monitor power-saving kicks in)
	HRESULT hr = m_lpDX->m_lpDevice->TestCooperativeLevel();
	if (hr == D3DERR_DEVICENOTRESET)
	{
		// device WAS lost, and is now ready to be reset (and come back online):
		CleanUpDX9Stuff(0);
		if (m_lpDX->m_lpDevice->Reset(&m_lpDX->m_d3dpp) != D3D_OK)
		{
			return false;  // EXIT THE PLUGIN
		}
		if (!AllocateDX9Stuff())
			return false;  // EXIT THE PLUGIN
	}
	else if (hr != D3D_OK)
	{
		// device is lost, and not yet ready to come back; sleep.
		Sleep(30);
		return true;
	}

	DoTime();
	AnalyzeNewSound(pWaveL, pWaveR);
	AlignWaves();

	DrawAndDisplay(0);

	EnforceMaxFPS();
	m_frame++;

	return true;
}


void CPluginShell::DrawAndDisplay(int redraw)
{
	if (D3D_OK==m_lpDX->m_lpDevice->BeginScene())
	{
		MyRenderFn(redraw);

		PrepareFor2DDrawing_B(GetDevice(), GetWidth(), GetHeight());

		m_lpDX->m_lpDevice->EndScene();
	}

	if (m_screenmode == WINDOWED && (m_lpDX->m_client_width != m_lpDX->m_REAL_client_width || m_lpDX->m_client_height != m_lpDX->m_REAL_client_height))
	{
		int real_w = m_lpDX->m_REAL_client_width;   // real client size, in pixels
		int real_h = m_lpDX->m_REAL_client_height;
		int fat_w = m_lpDX->m_client_width;         // oversized VS canvas size, in pixels
		int fat_h = m_lpDX->m_client_height;
		int extra_w = fat_w - real_w;
		int extra_h = fat_h - real_h;
		RECT src, dst;
		SetRect(&src, extra_w/2, extra_h/2, extra_w/2 + real_w, extra_h/2 + real_h);
		SetRect(&dst, 0, 0, real_w, real_h);
		m_lpDX->m_lpDevice->Present(&src, &dst,NULL,NULL);
	}
	else
		m_lpDX->m_lpDevice->Present(NULL,NULL,NULL,NULL);
}

void CPluginShell::EnforceMaxFPS()
{
	int max_fps;
	switch (m_screenmode)
	{
	case WINDOWED:        max_fps = m_max_fps_w;  break;
	case FULLSCREEN:      max_fps = m_max_fps_fs; break;
	case FAKE_FULLSCREEN: max_fps = m_max_fps_fs; break;
	}

	if (max_fps <= 0)
		return;

	float fps_lo = (float)max_fps;
	float fps_hi = (float)max_fps;

	if (m_save_cpu)
	{
		// Find the optimal lo/hi bounds for the fps
		// that will result in a maximum difference,
		// in the time for a single frame, of 0.003 seconds -
		// the assumed granularity for Sleep(1) -

		// Using this range of acceptable fps
		// will allow us to do (sloppy) fps limiting
		// using only Sleep(1), and never the
		// second half of it: Sleep(0) in a tight loop,
		// which sucks up the CPU (whereas Sleep(1)
		// leaves it idle).

		// The original equation:
		//   1/(max_fps*t1) = 1/(max*fps/t1) - 0.003
		// where:
		//   t1 > 0
		//   max_fps*t1 is the upper range for fps
		//   max_fps/t1 is the lower range for fps

		float a = 1;
		float b = -0.003f * max_fps;
		float c = -1.0f;
		float det = b*b - 4*a*c;
		if (det>0)
		{
			float t1 = (-b + sqrtf(det)) / (2*a);
			//float t2 = (-b - sqrtf(det)) / (2*a);

			if (t1 > 1.0f)
			{
				fps_lo = max_fps / t1;
				fps_hi = max_fps * t1;
				// verify: now [1.0f/fps_lo - 1.0f/fps_hi] should equal 0.003 seconds.
				// note: allowing tolerance to go beyond these values for
				// fps_lo and fps_hi would gain nothing.
			}
		}
	}

	if (m_high_perf_timer_freq.QuadPart > 0)
	{
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);

		if (m_prev_end_of_frame.QuadPart != 0)
		{
			int ticks_to_wait_lo = (int)((float)m_high_perf_timer_freq.QuadPart / (float)fps_hi);
			int ticks_to_wait_hi = (int)((float)m_high_perf_timer_freq.QuadPart / (float)fps_lo);
			int done = 0;
			int loops = 0;
			do
			{
				QueryPerformanceCounter(&t);

				__int64 t2 = t.QuadPart - m_prev_end_of_frame.QuadPart;
				if (t2 > 2147483000)
					done = 1;
				if (t.QuadPart < m_prev_end_of_frame.QuadPart)    // time wrap
					done = 1;

				// this is sloppy - if your freq. is high, this can overflow (to a (-) int) in just a few minutes
				// but it's ok, we have protection for that above.
				int ticks_passed = (int)(t.QuadPart - m_prev_end_of_frame.QuadPart);
				if (ticks_passed >= ticks_to_wait_lo)
					done = 1;

				if (!done)
				{
					// if > 0.01s left, do Sleep(1), which will actually sleep some
					//   steady amount of up to 3 ms (depending on the OS),
					//   and do so in a nice way (cpu meter drops; laptop battery spared).
					// otherwise, do a few Sleep(0)'s, which just give up the timeslice,
					//   but don't really save cpu or battery, but do pass a tiny
					//   amount of time.

					//if (ticks_left > (int)m_high_perf_timer_freq.QuadPart/500)
					if (ticks_to_wait_hi - ticks_passed > (int)m_high_perf_timer_freq.QuadPart/100)
						Sleep(5);
					else if (ticks_to_wait_hi - ticks_passed > (int)m_high_perf_timer_freq.QuadPart/1000)
						Sleep(1);
					else
						for (int i=0; i<10; i++)
							Sleep(0);  // causes thread to give up its timeslice
				}
			}
			while (!done);
		}

		m_prev_end_of_frame = t;
	}
	else
	{
		Sleep(1000/max_fps);
	}
}

void CPluginShell::DoTime()
{
	if (m_frame==0)
	{
		m_fps = 30;
		m_time = 0;
		m_time_hist_pos = 0;
	}

	double new_raw_time;
	float elapsed;

	if (m_high_perf_timer_freq.QuadPart != 0)
	{
		// get high-precision time
		// precision: usually from 1..6 us (MICROseconds), depending on the cpu speed.
		// (higher cpu speeds tend to have better precision here)
		LARGE_INTEGER t;
		if (!QueryPerformanceCounter(&t))
		{
			m_high_perf_timer_freq.QuadPart = 0;   // something went wrong (exception thrown) -> revert to crappy timer
		}
		else
		{
			new_raw_time = (double)t.QuadPart;
			elapsed = (float)((new_raw_time - m_last_raw_time)/(double)m_high_perf_timer_freq.QuadPart);
		}
	}

	if (m_high_perf_timer_freq.QuadPart == 0)
	{
		// get low-precision time
		// precision: usually 1 ms (MILLIsecond) for win98, and 10 ms for win2k.
		new_raw_time = (double)(timeGetTime()*0.001);
		elapsed = (float)(new_raw_time - m_last_raw_time);
	}

	m_last_raw_time = new_raw_time;
	int slots_to_look_back = (m_high_perf_timer_freq.QuadPart==0) ? TIME_HIST_SLOTS : TIME_HIST_SLOTS/2;

	m_time += 1.0f/m_fps;

	// timekeeping goals:
	//    1. keep 'm_time' increasing SMOOTHLY: (smooth animation depends on it)
	//          m_time += 1.0f/m_fps;     // where m_fps is a bit damped
	//    2. keep m_time_hist[] 100% accurate (except for filtering out pauses),
	//       so that when we look take the difference between two entries,
	//       we get the real amount of time that passed between those 2 frames.
	//          m_time_hist[i] = m_last_raw_time + elapsed_corrected;

	if (m_frame > TIME_HIST_SLOTS)
	{
		if (m_fps < 60.0f)
			slots_to_look_back = (int)(slots_to_look_back*(0.1f + 0.9f*(m_fps/60.0f)));

		if (elapsed > 5.0f/m_fps || elapsed > 1.0f || elapsed < 0)
			elapsed = 1.0f / 30.0f;

		float old_hist_time = m_time_hist[(m_time_hist_pos - slots_to_look_back + TIME_HIST_SLOTS) % TIME_HIST_SLOTS];
		float new_hist_time = m_time_hist[(m_time_hist_pos - 1 + TIME_HIST_SLOTS) % TIME_HIST_SLOTS]
		                      + elapsed;

		m_time_hist[m_time_hist_pos] = new_hist_time;
		m_time_hist_pos = (m_time_hist_pos+1) % TIME_HIST_SLOTS;

		float new_fps = slots_to_look_back / (float)(new_hist_time - old_hist_time);
		float damping = (m_high_perf_timer_freq.QuadPart==0) ? 0.93f : 0.87f;

		// damp heavily, so that crappy timer precision doesn't make animation jerky
		if (fabsf(m_fps - new_fps) > 3.0f)
			m_fps = new_fps;
		else
			m_fps = damping*m_fps + (1-damping)*new_fps;
	}
	else
	{
		float damping = (m_high_perf_timer_freq.QuadPart==0) ? 0.8f : 0.6f;

		if (m_frame < 2)
			elapsed = 1.0f / 30.0f;
		else if (elapsed > 1.0f || elapsed < 0)
			elapsed = 1.0f / m_fps;

		float old_hist_time = m_time_hist[0];
		float new_hist_time = m_time_hist[(m_time_hist_pos - 1 + TIME_HIST_SLOTS) % TIME_HIST_SLOTS]
		                      + elapsed;

		m_time_hist[m_time_hist_pos] = new_hist_time;
		m_time_hist_pos = (m_time_hist_pos+1) % TIME_HIST_SLOTS;

		if (m_frame > 0)
		{
			float new_fps = (m_frame) / (new_hist_time - old_hist_time);
			m_fps = damping*m_fps + (1-damping)*new_fps;
		}
	}

	// Synchronize the audio and video by telling Winamp how many milliseconds we want the audio data,
	// before it's actually audible.  If we set this to the amount of time it takes to display 1 frame
	// (1/fps), the video and audio should be perfectly synchronized.
	if (m_fps < 2.0f)
		mod1.latencyMs = 500;
	else if (m_fps > 125.0f)
		mod1.latencyMs = 8;
	else
		mod1.latencyMs = (int)(1000.0f/m_fps*m_lpDX->m_frame_delay + 0.5f);
}

void CPluginShell::AnalyzeNewSound(unsigned char *pWaveL, unsigned char *pWaveR)
{
	// we get 576 samples in from winamp.
	// the output of the fft has 'num_frequencies' samples,
	//   and represents the frequency range 0 hz - 22,050 hz.
	// usually, plugins only use half of this output (the range 0 hz - 11,025 hz),
	//   since >10 khz doesn't usually contribute much.

	int i;

	float temp_wave[2][576];

	int old_i = 0;
	for (i=0; i<576; i++)
	{
		m_sound.fWaveform[0][i] = (float)((pWaveL[i] ^ 128) - 128);
		m_sound.fWaveform[1][i] = (float)((pWaveR[i] ^ 128) - 128);

		// simulating single frequencies from 200 to 11,025 Hz:
		//float freq = 1.0f + 11050*(GetFrame() % 100)*0.01f;
		//m_sound.fWaveform[0][i] = 10*sinf(i*freq*6.28f/44100.0f);

		// damp the input into the FFT a bit, to reduce high-frequency noise:
		temp_wave[0][i] = 0.5f*(m_sound.fWaveform[0][i] + m_sound.fWaveform[0][old_i]);
		temp_wave[1][i] = 0.5f*(m_sound.fWaveform[1][i] + m_sound.fWaveform[1][old_i]);
		old_i = i;
	}

	m_fftobj.time_to_frequency_domain(temp_wave[0], m_sound.fSpectrum[0]);
	m_fftobj.time_to_frequency_domain(temp_wave[1], m_sound.fSpectrum[1]);

	// sum (left channel) spectrum up into 3 bands
	// [note: the new ranges do it so that the 3 bands are equally spaced, pitch-wise]
	float min_freq = 200.0f;
	float max_freq = 11025.0f;
	float net_octaves = (logf(max_freq/min_freq) / logf(2.0f));     // 5.7846348455575205777914165223593
	float octaves_per_band = net_octaves / 3.0f;                    // 1.9282116151858401925971388407864
	float mult = powf(2.0f, octaves_per_band); // each band's highest freq. divided by its lowest freq.; 3.805831305510122517035102576162
	// [to verify: min_freq * mult * mult * mult should equal max_freq.]
	for (int ch=0; ch<2; ch++)
	{
		for (i=0; i<3; i++)
		{
			// old guesswork code for this:
			//   float exp = 2.1f;
			//   int start = (int)(NUM_FREQUENCIES*0.5f*powf(i/3.0f, exp));
			//   int end   = (int)(NUM_FREQUENCIES*0.5f*powf((i+1)/3.0f, exp));
			// results:
			//          old range:      new range (ideal):
			//   bass:  0-1097          200-761
			//   mids:  1097-4705       761-2897
			//   treb:  4705-11025      2897-11025
			int start = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)i)/11025.0f);
			int end   = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)(i+1))/11025.0f);
			if (start < 0) start = 0;
			if (end > NUM_FREQUENCIES) end = NUM_FREQUENCIES;

			m_sound.imm[ch][i] = 0;
			for (int j=start; j<end; j++)
				m_sound.imm[ch][i] += m_sound.fSpectrum[ch][j];
			m_sound.imm[ch][i] /= (float)(end-start);
		}
	}

	// some code to find empirical long-term averages for imm[0..2]:
	/*{
	    static float sum[3];
	    static int count = 0;

	    #define FRAMES_PER_SONG 300     // should be at least 200!

	    if (m_frame < FRAMES_PER_SONG)
	    {
	        sum[0] = sum[1] = sum[2] = 0;
	        count = 0;
	    }
	    else
	    {
	        if (m_frame%FRAMES_PER_SONG == 0)
	        {
	            char buf[256];
	            sprintf(buf, "%.4f, %.4f, %.4f     (%d samples / ~%d songs)\n",
	                sum[0]/(float)(count),
	                sum[1]/(float)(count),
	                sum[2]/(float)(count),
	                count,
	                count/(FRAMES_PER_SONG-10)
	            );
	            OutputDebugString(buf);

	            // skip to next song
	            PostMessage(m_hWndWinamp,WM_COMMAND,40048,0);
	        }
	        else if (m_frame%FRAMES_PER_SONG == 5)
	        {
	            // then advance to 0-2 minutes into the song:
	            PostMessage(m_hWndWinamp,WM_USER,(20 + (warand()%65) + (rand()%65))*1000,106);
	        }
	        else if (m_frame%FRAMES_PER_SONG >= 10)
	        {
	            sum[0] += m_sound.imm[0];
	            sum[1] += m_sound.imm[1];
	            sum[2] += m_sound.imm[2];
	            count++;
	        }
	    }
	}*/

	// multiply by long-term, empirically-determined inverse averages:
	// (for a trial of 244 songs, 10 seconds each, somewhere in the 2nd or 3rd minute,
	//  the average levels were: 0.326781557	0.38087377	0.199888934
	for (ch=0; ch<2; ch++)
	{
		m_sound.imm[ch][0] /= 0.326781557f;//0.270f;
		m_sound.imm[ch][1] /= 0.380873770f;//0.343f;
		m_sound.imm[ch][2] /= 0.199888934f;//0.295f;
	}

	// do temporal blending to create attenuated and super-attenuated versions
	for (ch=0; ch<2; ch++)
	{
		for (i=0; i<3; i++)
		{
			// m_sound.avg[i]
			{
				float avg_mix;
				if (m_sound.imm[ch][i] > m_sound.avg[ch][i])
					avg_mix = AdjustRateToFPS(0.2f, 14.0f, m_fps);
				else
					avg_mix = AdjustRateToFPS(0.5f, 14.0f, m_fps);
				m_sound.avg[ch][i] = m_sound.avg[ch][i]*avg_mix + m_sound.imm[ch][i]*(1-avg_mix);
			}

			// m_sound.med_avg[i]
			// m_sound.long_avg[i]
			{
				float med_mix  = 0.91f;//0.800f + 0.11f*powf(t, 0.4f);    // primarily used for velocity_damping
				float long_mix = 0.96f;//0.800f + 0.16f*powf(t, 0.2f);    // primarily used for smoke plumes
				med_mix  = AdjustRateToFPS(med_mix, 14.0f, m_fps);
				long_mix = AdjustRateToFPS(long_mix, 14.0f, m_fps);
				m_sound.med_avg[ch][i]  =  m_sound.med_avg[ch][i]*(med_mix) + m_sound.imm[ch][i]*(1-med_mix);
				m_sound.long_avg[ch][i] = m_sound.long_avg[ch][i]*(long_mix) + m_sound.imm[ch][i]*(1-long_mix);
			}
		}
	}
}

void CPluginShell::PrepareFor2DDrawing_B(IDirect3DDevice9 *pDevice, int w, int h)
{
	// New 2D drawing area will have x,y coords in the range <-1,-1> .. <1,1>
	//         +--------+ Y=-1
	//         |        |
	//         | screen |             Z=0: front of scene
	//         |        |             Z=1: back of scene
	//         +--------+ Y=1
	//       X=-1      X=1
	// NOTE: After calling this, be sure to then call (at least):
	//  1. SetVertexShader()
	//  2. SetTexture(), if you need it
	// before rendering primitives!
	// Also, be sure your sprites have a z coordinate of 0.

	pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_ZFUNC,     D3DCMP_LESSEQUAL);
	pDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	pDevice->SetRenderState(D3DRS_FILLMODE,  D3DFILL_SOLID);
	pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
	pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_LOCALVIEWER, FALSE);
	pDevice->SetRenderState(D3DRS_COLORVERTEX, TRUE);

	pDevice->SetTexture(0, NULL);
	pDevice->SetTexture(1, NULL);
	pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);//D3DTEXF_LINEAR);
	pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);//D3DTEXF_LINEAR);
	pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	// set up for 2D drawing:
	{
		D3DXMATRIX Ortho2D;
		D3DXMATRIX Identity;

		D3DXMatrixOrthoLH(&Ortho2D, (float)w, (float)h, 0.0f, 1.0f);
		D3DXMatrixIdentity(&Identity);

		pDevice->SetTransform(D3DTS_PROJECTION, &Ortho2D);
		pDevice->SetTransform(D3DTS_WORLD, &Identity);
		pDevice->SetTransform(D3DTS_VIEW, &Identity);
	}
}

LRESULT CALLBACK CPluginShell::WindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam)
{
	//if (uMsg==WM_GETDLGCODE)
	//    return DLGC_WANTALLKEYS|DLGC_WANTCHARS|DLGC_WANTMESSAGE;    // this tells the embedwnd that we want keypresses to flow through to our client wnd.

	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT *create = (CREATESTRUCT *)lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
	}

	CPluginShell* p = (CPluginShell*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	if (p)
		return p->PluginShellWindowProc(hWnd, uMsg, wParam, lParam);
	else
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CPluginShell::PluginShellWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam)
{
	USHORT mask = 1 << (sizeof(SHORT)*8 - 1);
	//bool bShiftHeldDown = (GetKeyState(VK_SHIFT) & mask) != 0;
	bool bCtrlHeldDown  = (GetKeyState(VK_CONTROL) & mask) != 0;
	//bool bAltHeldDown: most keys come in under WM_SYSKEYDOWN when ALT is depressed.

	int i;
#ifdef _DEBUG
	char caption[256] = "WndProc: frame 0, ";
	if (m_frame > 0)
	{
		float time = m_time;
		int hours = (int)(time/3600);
		time -= hours*3600;
		int minutes = (int)(time/60);
		time -= minutes*60;
		int seconds = (int)time;
		time -= seconds;
		int dsec = (int)(time*100);
		sprintf(caption, "WndProc: frame %d, t=%dh:%02dm:%02d.%02ds, ", m_frame, hours, minutes, seconds, dsec);
	}

	if (uMsg != WM_MOUSEMOVE &&
	    uMsg != WM_NCHITTEST &&
	    uMsg != WM_SETCURSOR &&
	    uMsg != WM_COPYDATA &&
	    uMsg != WM_USER)
		OutputDebugMessage(caption, hWnd, uMsg, wParam, lParam);
#endif

	switch (uMsg)
	{
	case WM_USER:
		break;

	case WM_COPYDATA:
		break;

	case WM_ERASEBKGND:
		// Repaint window when song is paused and image needs to be repainted:
		if (SendMessage(m_hWndWinamp,WM_USER,0,104)!=1 && m_lpDX && m_lpDX->m_lpDevice && GetFrame() > 0)    // WM_USER/104 return codes: 1=playing, 3=paused, other=stopped
		{
			m_lpDX->m_lpDevice->Present(NULL,NULL,NULL,NULL);
			return 0;
		}
		break;

	case WM_WINDOWPOSCHANGING:
		if (m_screenmode==WINDOWED && m_lpDX && m_lpDX->m_ready && m_lpDX->m_current_mode.m_skin)
			m_lpDX->SaveWindow();
		break;
	case WM_NCACTIVATE:
		// *Very Important Handler!*
		//    -Without this code, the app would not work properly when running in true
		//     fullscreen mode on multiple monitors; it would auto-minimize whenever the
		//     user clicked on a window in another display.
		if (wParam == 0 &&
		    m_screenmode == FULLSCREEN &&
		    m_frame > 0 &&
		    !m_exiting &&
		    m_lpDX &&
		    m_lpDX->m_ready
		    && m_lpDX->m_lpD3D &&
		    m_lpDX->m_lpD3D->GetAdapterCount() > 1
		   )
		{
			return 0;
		}
		break;

	case WM_DESTROY:
		// note: don't post quit message here if the window is being destroyed
		// and re-created on a switch between windowed & FAKE fullscreen modes.
		if (!m_lpDX->TempIgnoreDestroyMessages())
		{
			// this is a final exit, and not just destroy-then-recreate-the-window.
			// so, flag DXContext so it knows that someone else
			// will take care of destroying the window!
			m_lpDX->OnTrulyExiting();
			PostQuitMessage(0);
		}
		return FALSE;
		break;
		// benski> a little hack to get the window size correct. it seems to work
	case WM_USER+555:
		if (m_lpDX && m_lpDX->m_ready && m_screenmode==WINDOWED && !m_resizing)
		{
			OnUserResizeWindow();
			m_lpDX->SaveWindow();
		}
		break;
	case WM_SIZE:
		// clear or set activity flag to reflect focus
		if (m_lpDX && m_lpDX->m_ready && m_screenmode==WINDOWED && !m_resizing)
		{
			m_hidden = (SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam) ? TRUE : FALSE;

			if (SIZE_MAXIMIZED==wParam || SIZE_RESTORED==wParam) // the window has been maximized or restored
				OnUserResizeWindow();
		}
		break;

	case WM_ENTERSIZEMOVE:
		m_resizing = 1;
		break;

	case WM_EXITSIZEMOVE:
		if (m_lpDX && m_lpDX->m_ready && m_screenmode==WINDOWED)
			OnUserResizeWindow();
		m_resizing = 0;
		break;

	case WM_GETMINMAXINFO:
	{
		// don't let the window get too small
		MINMAXINFO* p = (MINMAXINFO*)lParam;
		if (p->ptMinTrackSize.x < 64)
			p->ptMinTrackSize.x = 64;
		p->ptMinTrackSize.y = p->ptMinTrackSize.x*3/4;
	}
	return 0;

	case WM_MOUSEMOVE:
		break;

	case WM_LBUTTONUP:
		break;

	case WM_USER + 1666:
		if (wParam == 1 && lParam == 15)
		{
			if (m_screenmode == FULLSCREEN || m_screenmode == FAKE_FULLSCREEN)
				ToggleFullScreen();
		}
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		// Toggle between Fullscreen and Windowed modes on double-click
		// note: this requires the 'CS_DBLCLKS' windowclass style!
		{
			SetFocus(hWnd);
			if (uMsg==WM_LBUTTONDBLCLK && m_frame>0)
			{
				ToggleFullScreen();
				return 0;
			}
		}
		break;

	case WM_SETFOCUS:
		// note: this msg never comes in when embedwnd is used, but that's ok, because that's only
		// in Windowed mode, and m_lost_focus only makes us sleep when fullscreen.
		m_lost_focus = 0;
		break;

	case WM_KILLFOCUS:
		// note: this msg never comes in when embedwnd is used, but that's ok, because that's only
		// in Windowed mode, and m_lost_focus only makes us sleep when fullscreen.
		m_lost_focus = 1;
		break;

	case WM_SETCURSOR:
		if (
		  (m_screenmode == FULLSCREEN) ||
		  (m_screenmode == FAKE_FULLSCREEN && m_lpDX->m_fake_fs_covers_all)
		)
		{
			// hide the cursor
			SetCursor(NULL);
			return TRUE; // prevent Windows from setting cursor to window class cursor
		}
		break;

	case WM_NCHITTEST:
		// Prevent the user from selecting the menu in fullscreen mode
		if (m_screenmode != WINDOWED)
			return HTCLIENT;
		break;

	case WM_SYSCOMMAND:
		// Prevent *moving/sizing* and *entering standby mode* when in fullscreen mode
		switch (wParam)
		{
		case SC_MOVE:
		case SC_SIZE:
		case SC_MAXIMIZE:
		case SC_KEYMENU:
			if (m_screenmode != WINDOWED)
				return 1;
			break;
		case SC_MONITORPOWER:
			if (m_screenmode == FULLSCREEN || m_screenmode == FAKE_FULLSCREEN)
				return 1;
			break;
		}
		break;

	case WM_CONTEXTMENU:
		break;

	case WM_COMMAND:
		// handle clicks on items on context menu.
		if (m_screenmode == WINDOWED)
		{
			switch (LOWORD(wParam))
			{
			case ID_QUIT:
				m_exiting = 1;
				PostMessage(hWnd, WM_CLOSE, 0, 0);
				return 0;
			case ID_GO_FS:
				if (m_frame > 0)
					ToggleFullScreen();
				return 0;
			case ID_DESKTOP_MODE:
				return 0;
			}
			// then allow the plugin to override any command:
			if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
				return 0;
		}
		break;

		/*
		KEY HANDLING: the basic idea:
		    -in all cases, handle or capture:
		        -ZXCVBRS, zxcvbrs
		            -also make sure it's case-insensitive!  (lowercase come through only as WM_CHAR; uppercase come in as both)
		        -(ALT+ENTER)
		        -(F1, ESC, UP, DN, Left, Right, SHIFT+l/r)
		        -(P for playlist)
		            -when playlist showing: steal J, HOME, END, PGUP, PGDN, UP, DOWN, ESC
		        -(BLOCK J, L)
		    -when integrated with winamp (using embedwnd), also handle these keys:
		        -j, l, L, CTRL+L [windowed mode only!]
		        -CTRL+P, CTRL+D
		        -CTRL+TAB
		        -ALT-E
		        -ALT+F (main menu)
		        -ALT+3 (id3)
		*/

	case WM_SYSKEYDOWN:
		if (wParam==VK_RETURN && m_frame > 0)
		{
			ToggleFullScreen();
			return 0;
		}
		// if in embedded mode (using winamp skin), pass ALT+ keys on to winamp
		// ex: ALT+E, ALT+F, ALT+3...
		if (m_screenmode==WINDOWED && m_lpDX->m_current_mode.m_skin)
			return PostMessage(m_hWndWinamp, uMsg, wParam, lParam); // force-pass to winamp; required for embedwnd
		break;

	case WM_SYSKEYUP:
		if (m_screenmode==WINDOWED && m_lpDX->m_current_mode.m_skin)
			return PostMessage(m_hWndWinamp, uMsg, wParam, lParam); // force-pass to winamp; required for embedwnd
		break;

	case WM_SYSCHAR:
		break;

	case WM_CHAR:
		// then allow the plugin to override any keys:
		if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
			return 0;

		// finally, default key actions:
		if (wParam == 'z' || wParam == 'Z')	// 'z' or 'Z'
		{
			PostMessage(m_hWndWinamp,WM_COMMAND,40044,0);
			return 0;
		}
		else
			{
			switch (wParam)
			{
				// WINAMP PLAYBACK CONTROL KEYS:
			case 'x':
			case 'X':
				PostMessage(m_hWndWinamp,WM_COMMAND,40045,0);
				return 0;
			case 'c':
			case 'C':
				PostMessage(m_hWndWinamp,WM_COMMAND,40046,0);
				return 0;
			case 'v':
			case 'V':
				PostMessage(m_hWndWinamp,WM_COMMAND,40047,0);
				return 0;
			case 'b':
			case 'B':
				PostMessage(m_hWndWinamp,WM_COMMAND,40048,0);
				return 0;
			case 's':
			case 'S':
				//if (SendMessage(m_hWndWinamp,WM_USER,0,250))
				//    sprintf(m_szUserMessage, "shuffle is now OFF");    // shuffle was on
				//else
				//    sprintf(m_szUserMessage, "shuffle is now ON");    // shuffle was off

				// toggle shuffle
				PostMessage(m_hWndWinamp,WM_COMMAND,40023,0);
				return 0;
			case 'r':
			case 'R':
				// toggle repeat
				PostMessage(m_hWndWinamp,WM_COMMAND,40022,0);
				return 0;
			case 'l':
				// note that this is actually correct; when you hit 'l' from the
				// MAIN winamp window, you get an "open files" dialog; when you hit
				// 'l' from the playlist editor, you get an "add files to playlist" dialog.
				// (that sends IDC_PLAYLIST_ADDMP3==1032 to the playlist, which we can't
				//  do from here.)
				PostMessage(m_hWndWinamp,WM_COMMAND,40029,0);
				return 0;
			case 'L':
				PostMessage(m_hWndWinamp,WM_COMMAND,40187,0);
				return 0;
			case 'j':
				PostMessage(m_hWndWinamp,WM_COMMAND,40194,0);
				return 0;
			}

			return 0;//DefWindowProc(hWnd,uMsg,wParam,lParam);
		}
		break;  // end case WM_CHAR

	case WM_KEYUP:

		// allow the plugin to override any keys:
		if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
			return 0;

		/*
		switch(wParam)
		{
		case VK_SOMETHING:
		    ...
		    break;
		}
		*/

		return 0;
		break;

	case WM_KEYDOWN:
		// allow the plugin to override any keys:
		if (MyWindowProc(hWnd, uMsg, wParam, lParam) == 0)
			return 0;

		switch (wParam)
		{
		case VK_F1:
			return 0;

		case VK_ESCAPE:
			{
				if (m_screenmode == FAKE_FULLSCREEN || m_screenmode == FULLSCREEN)
				{
					ToggleFullScreen();
				}
				// exit the program on escape
				//m_exiting = 1;
				//PostMessage(hWnd, WM_CLOSE, 0, 0);
			}
			return 0;

		case VK_UP:
			// increase volume
		{
			int nRepeat = lParam & 0xFFFF;
			for (i=0; i<nRepeat*2; i++) PostMessage(m_hWndWinamp,WM_COMMAND,40058,0);
		}
		return 0;

		case VK_DOWN:
			// decrease volume
		{
			int nRepeat = lParam & 0xFFFF;
			for (i=0; i<nRepeat*2; i++) PostMessage(m_hWndWinamp,WM_COMMAND,40059,0);
		}
		return 0;

		case VK_LEFT:
		case VK_RIGHT:
		{
			bool bShiftHeldDown = (GetKeyState(VK_SHIFT) & mask) != 0;
			int cmd = (wParam == VK_LEFT) ? 40144 : 40148;
			int nRepeat = lParam & 0xFFFF;
			int reps = (bShiftHeldDown) ? 6*nRepeat : 1*nRepeat;

			for (int i=0; i<reps; i++)
				PostMessage(m_hWndWinamp,WM_COMMAND,cmd,0);
		}
		return 0;
		default:
			// pass CTRL+A thru CTRL+Z, and also CTRL+TAB, to winamp, *if we're in windowed mode* and using an embedded window.
			// be careful though; uppercase chars come both here AND to WM_CHAR handler,
			//   so we have to eat some of them here, to avoid them from acting twice.
			if (m_screenmode==WINDOWED && m_lpDX && m_lpDX->m_current_mode.m_skin)
			{
				if (bCtrlHeldDown && ((wParam >= 'A' && wParam <= 'Z') || wParam==VK_TAB))
				{
					PostMessage(m_hWndWinamp, uMsg, wParam, lParam);
					return 0;
				}
			}
			return 0;
		}

		return 0;
		break;
	}

	return MyWindowProc(hWnd, uMsg, wParam, lParam);//DefWindowProc(hWnd, uMsg, wParam, lParam);
	//return 0L;
}

void CPluginShell::AlignWaves()
{
	// align waves, using recursive (mipmap-style) least-error matching
	// note: NUM_WAVEFORM_SAMPLES must be between 32 and 576.

	int align_offset[2] = { 0, 0 };

#if (NUM_WAVEFORM_SAMPLES < 576) // [don't let this code bloat our DLL size if it's not going to be used]

	int nSamples = NUM_WAVEFORM_SAMPLES;

#define MAX_OCTAVES 10

	int octaves = (int)floorf(logf((float)(576-nSamples))/logf(2.0f));
	if (octaves < 4)
		return;
	if (octaves > MAX_OCTAVES)
		octaves = MAX_OCTAVES;

	for (int ch=0; ch<2; ch++)
	{
		// only worry about matching the lower 'nSamples' samples
		float temp_new[MAX_OCTAVES][576];
		float temp_old[MAX_OCTAVES][576];
		static float temp_weight[MAX_OCTAVES][576];
		static int   first_nonzero_weight[MAX_OCTAVES];
		static int   last_nonzero_weight[MAX_OCTAVES];
		int spls[MAX_OCTAVES];
		int space[MAX_OCTAVES];

		memcpy(temp_new[0], m_sound.fWaveform[ch], sizeof(float)*576);
		memcpy(temp_old[0], &m_oldwave[ch][m_prev_align_offset[ch]], sizeof(float)*nSamples);
		spls[0] = 576;
		space[0] = 576 - nSamples;

		// potential optimization: could reuse (instead of recompute) mip levels for m_oldwave[2][]?
		for (int octave=1; octave<octaves; octave++)
		{
			spls[octave] = spls[octave-1]/2;
			space[octave] = space[octave-1]/2;
			for (int n=0; n<spls[octave]; n++)
			{
				temp_new[octave][n] = 0.5f*(temp_new[octave-1][n*2] + temp_new[octave-1][n*2+1]);
				temp_old[octave][n] = 0.5f*(temp_old[octave-1][n*2] + temp_old[octave-1][n*2+1]);
			}
		}

		if (!m_align_weights_ready)
		{
			m_align_weights_ready = 1;
			for (octave=0; octave<octaves; octave++)
			{
				int compare_samples = spls[octave] - space[octave];
				for (int n=0; n<compare_samples; n++)
				{
					// start with pyramid-shaped pdf, from 0..1..0
					if (n < compare_samples/2)
						temp_weight[octave][n] = n*2/(float)compare_samples;
					else
						temp_weight[octave][n] = (compare_samples-1 - n)*2/(float)compare_samples;

					// TWEAK how much the center matters, vs. the edges:
					temp_weight[octave][n] = (temp_weight[octave][n] - 0.8f)*5.0f + 0.8f;

					// clip:
					if (temp_weight[octave][n]>1) temp_weight[octave][n] = 1;
					if (temp_weight[octave][n]<0) temp_weight[octave][n] = 0;
				}

				n = 0;
				while (temp_weight[octave][n] == 0 && n < compare_samples)
					n++;
				first_nonzero_weight[octave] = n;

				n = compare_samples-1;
				while (temp_weight[octave][n] == 0 && n >= 0)
					n--;
				last_nonzero_weight[octave] = n;
			}
		}

		int n1 = 0;
		int n2 = space[octaves-1];
		for (octave = octaves-1; octave>=0; octave--)
		{
			// for example:
			//  space[octave] == 4
			//  spls[octave] == 36
			//  (so we test 32 samples, w/4 offsets)
			int compare_samples = spls[octave]-space[octave];

			int lowest_err_offset = -1;
			float lowest_err_amount = 0;
			for (int n=n1; n<n2; n++)
			{
				float err_sum = 0;
				//for (int i=0; i<compare_samples; i++)
				for (int i=first_nonzero_weight[octave]; i<=last_nonzero_weight[octave]; i++)
				{
					float x = (temp_new[octave][i+n] - temp_old[octave][i]) * temp_weight[octave][i];
					if (x>0)
						err_sum += x;
					else
						err_sum -= x;
				}

				if (lowest_err_offset == -1 || err_sum < lowest_err_amount)
				{
					lowest_err_offset = n;
					lowest_err_amount = err_sum;
				}
			}

			// now use 'lowest_err_offset' to guide bounds of search in next octave:
			//  space[octave] == 8
			//  spls[octave] == 72
			//     -say 'lowest_err_offset' was 2
			//     -that corresponds to samples 4 & 5 of the next octave
			//     -also, expand about this by 2 samples?  YES.
			//  (so we'd test 64 samples, w/8->4 offsets)
			if (octave > 0)
			{
				n1 = lowest_err_offset*2  -1;
				n2 = lowest_err_offset*2+2+1;
				if (n1 < 0) n1=0;
				if (n2 > space[octave-1]) n2 = space[octave-1];
			}
			else
				align_offset[ch] = lowest_err_offset;
		}
	}
#endif
	memcpy(m_oldwave[0], m_sound.fWaveform[0], sizeof(float)*576);
	memcpy(m_oldwave[1], m_sound.fWaveform[1], sizeof(float)*576);
	m_prev_align_offset[0] = align_offset[0];
	m_prev_align_offset[1] = align_offset[1];

	// finally, apply the results: modify m_sound.fWaveform[2][0..576]
	// by scooting the aligned samples so that they start at m_sound.fWaveform[2][0].
	for (ch=0; ch<2; ch++)
		if (align_offset[ch]>0)
		{
			for (int i=0; i<nSamples; i++)
				m_sound.fWaveform[ch][i] = m_sound.fWaveform[ch][i+align_offset[ch]];
			// zero the rest out, so it's visually evident that these samples are now bogus:
			memset(&m_sound.fWaveform[ch][nSamples], 0, (576-nSamples)*sizeof(float));
		}
}
