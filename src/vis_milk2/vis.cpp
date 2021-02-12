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

#include "api.h"
#include <locale.h>
#include <windows.h>
#include "vis.h"
#include "plugin.h"
#include "defines.h"
#include "resource.h"
#include "utility.h"

CPlugin  g_plugin;
_locale_t g_use_C_locale = 0;
bool g_bFullyExited = true;

void config(struct winampVisModule *this_mod); // configuration dialog
int init(struct winampVisModule *this_mod);       // initialization for module
int render1(struct winampVisModule *this_mod);  // rendering for module 1
void quit(struct winampVisModule *this_mod);   // deinitialization for module

// our only plugin module in this plugin:
winampVisModule mod1 =
{
    MODULEDESC,
    NULL,	// hwndParent
    NULL,	// hDllInstance
    0,		// sRate
    0,		// nCh
    0,		// latencyMS - tells winamp how much in advance you want the audio data, 
			//             in ms.
    10,		// delayMS - if winamp tells the plugin to render a frame and it takes
			//           less than this # of milliseconds, winamp will sleep (go idle)
            //           for the remainder.  In effect, this limits the framerate of
            //           the plugin.  A value of 10 would cause a fps limit of ~100.
            //           Derivation: (1000 ms/sec) / (10 ms/frame) = 100 fps.
    0,		// spectrumNch
    2,		// waveformNch
    { 0, },	// spectrumData
    { 0, },	// waveformData
    config,
    init,
    render1, 
    quit
};

// getmodule routine from the main header. Returns NULL if an invalid module was requested,
// otherwise returns either mod1, mod2 or mod3 depending on 'which'.
winampVisModule *getModule(int which)
{
    switch (which)
    {
        case 0: return &mod1;
        //case 1: return &mod2;
        //case 2: return &mod3;
        default: return NULL;
    }
}

// Module header, includes version, description, and address of the module retriever function
winampVisHeader hdr = { VIS_HDRVER, DLLDESC, getModule };

// this is the only exported symbol. returns our main header.
// if you are compiling C++, the extern "C" { is necessary, so we just #ifdef it
#ifdef __cplusplus
extern "C" {
#endif
	__declspec( dllexport ) winampVisHeader *winampVisGetHeader(HWND hwndParent)
	{
		g_use_C_locale = _get_current_locale(); 

		return &hdr;
	}
#ifdef __cplusplus
}
#endif
 
bool WaitUntilPluginFinished(HWND hWndWinamp)
{
    int slept = 0;
    while (!g_bFullyExited && slept < 1000)
    {
        Sleep(50);
        slept += 50;
    }

    if (!g_bFullyExited)
    {
		wchar_t title[64];
        MessageBoxW(hWndWinamp, WASABI_API_LNGSTRINGW(IDS_ERROR_THE_PLUGIN_IS_ALREADY_RUNNING),
				    WASABI_API_LNGSTRINGW_BUF(IDS_MILKDROP_ERROR, title, 64),
				    MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
        return false;
    }

    return true;
}

// configuration. Passed this_mod, as a "this" parameter. Allows you to make one configuration
// function that shares code for all your modules (you don't HAVE to use it though, you can make
// config1(), config2(), etc...)
void config(struct winampVisModule *this_mod)
{
	return;
}

int (*warand)(void) = 0;

int fallback_rand_fn(void) {
  return rand();
}

// initialization. Registers our window class, creates our window, etc. Again, this one works for
// both modules, but you could make init1() and init2()...
// returns 0 on success, 1 on failure.
int init(struct winampVisModule *this_mod)
{
	//warand = fallback_rand_fn;
	
	if (!warand)
    {
		warand = (int (*)(void))SendMessage(this_mod->hwndParent, WM_WA_IPC, 0, IPC_GET_RANDFUNC);
        if ((size_t)warand <= 1)
        {
            warand = fallback_rand_fn;
        }
    }
    
    if (!WaitUntilPluginFinished(this_mod->hwndParent))
    {
        return 1;        
    }

    g_bFullyExited = false;

    if (!g_plugin.PluginPreInitialize(this_mod->hwndParent, this_mod->hDllInstance))
    {
        g_plugin.PluginQuit();
        g_bFullyExited = true;
        return 1;
    }

    if (!g_plugin.PluginInitialize())
    {
        g_plugin.PluginQuit();
        g_bFullyExited = true;
        return 1;
    }

    return 0;    // success
}

// render function for oscilliscope. Returns 0 if successful, 1 if visualization should end.
int render1(struct winampVisModule *this_mod)
{
	if (g_plugin.PluginRender(this_mod->waveformData[0], this_mod->waveformData[1]))
		return 0;    // ok
	else
		return 1;    // failed
}

// cleanup (opposite of init()). Should destroy the window, unregister the window class, etc.
void quit(struct winampVisModule *this_mod)
{
	g_plugin.PluginQuit();
	g_bFullyExited = true;
}