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

#ifndef __NULLSOFT_DX9_PLUGIN_SHELL_H__
#define __NULLSOFT_DX9_PLUGIN_SHELL_H__ 1

#include "shell_defines.h"
#include "dxcontext.h"
#include "fft.h"
#include "defines.h"
#include "md_defines.h"

#include "Vector.h"
#include "AutoWide.h"

#define TIME_HIST_SLOTS 128     // # of slots used if fps > 60.  half this many if fps==30.
#define MAX_SONGS_PER_PAGE 40

typedef struct
{
    float   imm[2][3];                // bass, mids, treble, no damping, for each channel (long-term average is 1)
    float    avg[2][3];               // bass, mids, treble, some damping, for each channel (long-term average is 1)
    float     med_avg[2][3];          // bass, mids, treble, more damping, for each channel (long-term average is 1)
    float      long_avg[2][3];        // bass, mids, treble, heavy damping, for each channel (long-term average is 1)
    float       infinite_avg[2][3];   // bass, mids, treble: winamp's average output levels. (1)
    float   fWaveform[2][576];             // Not all 576 are valid! - only NUM_WAVEFORM_SAMPLES samples are valid for each channel (note: NUM_WAVEFORM_SAMPLES is declared in shell_defines.h)
    float   fSpectrum[2][NUM_FREQUENCIES]; // NUM_FREQUENCIES samples for each channel (note: NUM_FREQUENCIES is declared in shell_defines.h)
} td_soundinfo;                    // ...range is 0 Hz to 22050 Hz, evenly spaced.

class CPluginShell
{
public:    
    // GET METHODS
    // ------------------------------------------------------------
    int       GetFrame();          // returns current frame # (starts at zero)
    float     GetTime();           // returns current animation time (in seconds) (starts at zero) (updated once per frame)
    float     GetFps();            // returns current estimate of framerate (frames per second)
    eScrMode  GetScreenMode();     // returns WINDOWED, FULLSCREEN, FAKE_FULLSCREEN, DESKTOP, or NOT_YET_KNOWN (if called before or during OverrideDefaults()).
    HWND      GetWinampWindow();   // returns handle to Winamp main window
    HINSTANCE GetInstance();       // returns handle to the plugin DLL module; used for things like loading resources (dialogs, bitmaps, icons...) that are built into the plugin.
    wchar_t*  GetPluginsDirPath(); // usually returns 'c:\\program files\\winamp\\plugins\\'
    wchar_t*  GetConfigIniFile();  // usually returns 'c:\\program files\\winamp\\plugins\\something.ini' - filename is determined from identifiers in 'defines.h'
	char*     GetConfigIniFileA();
protected:

    // GET METHODS THAT ONLY WORK ONCE DIRECTX IS READY
    // ------------------------------------------------------------
    //  The following 'Get' methods are only available after DirectX has been initialized.
    //  If you call these from OverrideDefaults, MyPreInitialize, or MyReadConfig, 
    //    they will return NULL (zero).
    // ------------------------------------------------------------
    HWND         GetPluginWindow();      // returns handle to the plugin window.  NOT persistent; can change!  
    int          GetWidth();             // returns width of plugin window interior, in pixels.  Note: in windowed mode, this is a fudged, larger, aligned value, and on final display, it gets cropped.
    int          GetHeight();            // returns height of plugin window interior, in pixels. Note: in windowed mode, this is a fudged, larger, aligned value, and on final display, it gets cropped.
    int          GetBitDepth();          // returns 8, 16, 24 (rare), or 32
    LPDIRECT3DDEVICE9  GetDevice();      // returns a pointer to the DirectX 8 Device.  NOT persistent; can change!
    D3DCAPS9*    GetCaps();              // returns a pointer to the D3DCAPS9 structer for the device.  NOT persistent; can change.
    D3DFORMAT    GetBackBufFormat();     // returns the pixelformat of the back buffer (probably D3DFMT_R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_R3G3B2, D3DFMT_A8R3G3B2, D3DFMT_X4R4G4B4, or D3DFMT_UNKNOWN)
    D3DFORMAT    GetBackBufZFormat();    // returns the pixelformat of the back buffer's Z buffer (probably D3DFMT_D16_LOCKABLE, D3DFMT_D32, D3DFMT_D15S1, D3DFMT_D24S8, D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D24X4S4, or D3DFMT_UNKNOWN)
    char*        GetDriverFilename();    // returns a text string with the filename of the current display adapter driver, such as "nv4_disp.dll"
    char*        GetDriverDescription(); // returns a text string describing the current display adapter, such as "NVIDIA GeForce4 Ti 4200"

protected:

    // MISC
    // ------------------------------------------------------------
    td_soundinfo m_sound;                   // a structure always containing the most recent sound analysis information; defined in pluginshell.h.

    // CONFIG PANEL SETTINGS
    // ------------------------------------------------------------
    // *** only read/write these values during CPlugin::OverrideDefaults! ***
    int          m_fake_fullscreen_mode;    // 0 or 1
    int          m_max_fps_fs;              // 1-120, or 0 for 'unlimited'
    int          m_max_fps_dm;              // 1-120, or 0 for 'unlimited'
    int          m_max_fps_w;               // 1-120, or 0 for 'unlimited'
    int          m_show_press_f1_msg;       // 0 or 1
    int          m_allow_page_tearing_w;    // 0 or 1
    int          m_allow_page_tearing_fs;   // 0 or 1
    int          m_allow_page_tearing_dm;   // 0 or 1
    int          m_minimize_winamp;         // 0 or 1
    int          m_dualhead_horz;           // 0 = both, 1 = left, 2 = right
    int          m_dualhead_vert;           // 0 = both, 1 = top, 2 = bottom
    int          m_save_cpu;                // 0 or 1
    int          m_skin;                    // 0 or 1
    int          m_fix_slow_text;           // 0 or 1
    D3DDISPLAYMODE m_disp_mode_fs;          // a D3DDISPLAYMODE struct that specifies the width, height, refresh rate, and color format to use when the plugin goes fullscreen.

    // PURE VIRTUAL FUNCTIONS (...must be implemented by derived classes)
    // ------------------------------------------------------------
    virtual void OverrideDefaults()      = 0;
    virtual void MyPreInitialize()       = 0;
    virtual void MyReadConfig()          = 0;
    virtual void MyWriteConfig()         = 0;
    virtual int  AllocateMyNonDx9Stuff() = 0;
    virtual void  CleanUpMyNonDx9Stuff() = 0;
    virtual int  AllocateMyDX9Stuff()    = 0;
    virtual void  CleanUpMyDX9Stuff(int final_cleanup) = 0;
    virtual void MyRenderFn(int redraw)  = 0;
    virtual LRESULT MyWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam) = 0;

//=====================================================================================================================
private:

    // GENERAL PRIVATE STUFF
    eScrMode     m_screenmode;      // // WINDOWED, FULLSCREEN, or FAKE_FULLSCREEN (i.e. running in a full-screen-sized window)
    int          m_frame;           // current frame #, starting at zero
    float        m_time;            // current animation time in seconds; starts at zero.
    float        m_fps;             // current estimate of frames per second
    HWND         m_hWndWinamp;      // handle to Winamp window 
    HINSTANCE    m_hInstance;       // handle to application instance
    DXContext*   m_lpDX;            // pointer to DXContext object
    wchar_t      m_szPluginsDirPath[MAX_PATH];  // usually 'c:\\program files\\winamp\\plugins\\'
    wchar_t      m_szImgIniFile[MAX_PATH];
    wchar_t      m_szConfigIniFile[MAX_PATH];   // usually 'c:\\program files\\winamp\\plugins\\something.ini' - filename is determined from identifiers in 'defines.h'
	char         m_szConfigIniFileA[MAX_PATH];   // usually 'c:\\program files\\winamp\\plugins\\something.ini' - filename is determined from identifiers in 'defines.h'
    
    // PRIVATE CONFIG PANEL SETTINGS
    D3DMULTISAMPLE_TYPE m_multisample_fullscreen;
    D3DMULTISAMPLE_TYPE m_multisample_desktop;
    D3DMULTISAMPLE_TYPE m_multisample_windowed;
    GUID m_adapter_guid_fullscreen;
    GUID m_adapter_guid_desktop;
    GUID m_adapter_guid_windowed;
    char m_adapter_devicename_fullscreen[256];  // these are also necessary sometimes,
    char m_adapter_devicename_desktop[256];     //  for example, when a laptop (single adapter)
    char m_adapter_devicename_windowed[256];    //  drives two displays!  DeviceName will be \\.\Display1 and \\.\Display2 or something.

    // PRIVATE RUNTIME SETTINGS
    int m_lost_focus;     // ~mostly for fullscreen mode
    int m_hidden;         // ~mostly for windowed mode
    int m_resizing;       // ~mostly for windowed mode
    int m_exiting;

    // PRIVATE - MORE TIMEKEEPING
   protected:
    double m_last_raw_time;
    LARGE_INTEGER m_high_perf_timer_freq;  // 0 if high-precision timer not available
   private:
    float  m_time_hist[TIME_HIST_SLOTS];		// cumulative
    int    m_time_hist_pos;
    LARGE_INTEGER m_prev_end_of_frame;

    // PRIVATE AUDIO PROCESSING DATA
    FFT   m_fftobj;
    float m_oldwave[2][576];        // for wave alignment
    int   m_prev_align_offset[2];   // for wave alignment
    int   m_align_weights_ready;

public:
    CPluginShell();
    ~CPluginShell();
    
    // called by vis.cpp, on behalf of Winamp:
    int  PluginPreInitialize(HWND hWinampWnd, HINSTANCE hWinampInstance);    
    int  PluginInitialize();                                                
    int  PluginRender(unsigned char *pWaveL, unsigned char *pWaveR);
    void PluginQuit();

    // config panel / windows messaging processes:
    static LRESULT CALLBACK WindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK DesktopWndProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ConfigDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    static INT_PTR CALLBACK TabCtrlProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    static INT_PTR CALLBACK FontDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    static INT_PTR CALLBACK DesktopOptionsDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    static INT_PTR CALLBACK DualheadDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

private:
    void PushWindowToJustBeforeDesktop(HWND h);
    void DrawAndDisplay(int redraw);
    void ReadConfig();
    void WriteConfig();
    void DoTime();
    void AnalyzeNewSound(unsigned char *pWaveL, unsigned char *pWaveR);
    void AlignWaves();
    int  InitDirectX();
    void CleanUpDirectX();
    int  AllocateDX9Stuff();
    void CleanUpDX9Stuff(int final_cleanup);
    int  InitNondx9Stuff();
    void CleanUpNondx9Stuff();
    void OnUserResizeWindow();
    void OnUserResizeTextWindow();
    void PrepareFor2DDrawing_B(IDirect3DDevice9 *pDevice, int w, int h);
public:
    void ToggleFullScreen();
protected:
    void StuffParams(DXCONTEXT_PARAMS *pParams);
    void EnforceMaxFPS();

    // WINDOWPROC FUNCTIONS
    LRESULT PluginShellWindowProc(HWND hWnd, unsigned uMsg, WPARAM wParam, LPARAM lParam);   // in windowproc.cpp

    // CONFIG PANEL FUNCTIONS:
    BOOL    PluginShellConfigDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    BOOL    PluginShellConfigTab1Proc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    BOOL    PluginShellFontDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    BOOL    PluginShellDesktopOptionsDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    BOOL    PluginShellDualheadDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    bool    InitConfig(HWND hDialogWnd);
    void    EndConfig();
    void    UpdateAdapters(int screenmode);
    void    UpdateFSAdapterDispModes();   // (fullscreen only)
    void    UpdateDispModeMultiSampling(int screenmode);
    void    UpdateMaxFps(int screenmode);
    int     GetCurrentlySelectedAdapter(int screenmode);
    void    SaveDisplayMode();
    void    SaveMultiSamp(int screenmode);
    void    SaveAdapter(int screenmode);
    void    SaveMaxFps(int screenmode);
    void    OnTabChanged(int nNewTab);
	LPDIRECT3DDEVICE9 GetTextDevice() { return m_lpDX->m_lpDevice; }

    // CHANGES:
    friend class CShaderParams;
};

#endif