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
#include "utility.h"
#include <math.h>
#include <locale.h>
#include <windows.h>
#include "wa_ipc.h"
#include "resource.h"

#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"d3dx9.lib")

float PowCosineInterp(float x, float pow)
{
    // input (x) & output should be in range 0..1.
    // pow > 0: tends to push things toward 0 and 1
    // pow < 0: tends to push things toward 0.5.

    if (x<0) 
        return 0;
    if (x>1)
        return 1;

    int bneg = (pow < 0) ? 1 : 0;
    if (bneg)
        pow = -pow;

    if (pow>1000) pow=1000;

    int its = (int)pow;
    for (int i=0; i<its; i++)
    {
        if (bneg)
            x = InvCosineInterp(x);
        else
            x = CosineInterp(x);
    }
    float x2 = (bneg) ? InvCosineInterp(x) : CosineInterp(x);
    float dx = pow - its;
    return ((1-dx)*x + (dx)*x2);
}

float AdjustRateToFPS(float per_frame_decay_rate_at_fps1, float fps1, float actual_fps)
{
    // returns the equivalent per-frame decay rate at actual_fps

    // basically, do all your testing at fps1 and get a good decay rate;
    // then, in the real application, adjust that rate by the actual fps each time you use it.
    
    float per_second_decay_rate_at_fps1 = powf(per_frame_decay_rate_at_fps1, fps1);
    float per_frame_decay_rate_at_fps2 = powf(per_second_decay_rate_at_fps1, 1.0f/actual_fps);

    return per_frame_decay_rate_at_fps2;
}

float GetPrivateProfileFloatW(wchar_t *szSectionName, wchar_t *szKeyName, float fDefault, wchar_t *szIniFile)
{
    wchar_t string[64];
    wchar_t szDefault[64];
    float ret = fDefault;

    _swprintf_l(szDefault, L"%f", g_use_C_locale, fDefault);

    if (GetPrivateProfileStringW(szSectionName, szKeyName, szDefault, string, 64, szIniFile) > 0)
    {
        _swscanf_l(string, L"%f", g_use_C_locale, &ret);
    }
    return ret;
}

bool WritePrivateProfileFloatW(float f, wchar_t *szKeyName, wchar_t *szIniFile, wchar_t *szSectionName)
{
    wchar_t szValue[32];
    _swprintf_l(szValue, L"%f", g_use_C_locale, f);
    return (WritePrivateProfileStringW(szSectionName, szKeyName, szValue, szIniFile) != 0);
}

bool WritePrivateProfileIntW(int d, wchar_t *szKeyName, wchar_t *szIniFile, wchar_t *szSectionName)
{
    wchar_t szValue[32];
    swprintf(szValue, L"%d", d);
    return (WritePrivateProfileStringW(szSectionName, szKeyName, szValue, szIniFile) != 0);
}


void RemoveExtension(wchar_t *str)
{
    wchar_t *p = wcsrchr(str, L'.');
    if (p) *p = 0;
}

static void ShiftDown(wchar_t *str)
{
	while (*str)
	{
		str[0] = str[1];
		str++;
	}
}

void RemoveSingleAmpersands(wchar_t *str)
{
	while (*str)
	{
		if (str[0] == L'&')
		{
			if (str[1] == L'&') // two in a row: replace with single ampersand, move on
				str++;

			ShiftDown(str);
		}
		else
			str = CharNextW(str);
	}
}

void TextToGuid(char *str, GUID *pGUID)
{
    if (!str) return;
    if (!pGUID) return;

    DWORD d[11];
    
    sscanf(str, "%X %X %X %X %X %X %X %X %X %X %X", 
        &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9], &d[10]);

    pGUID->Data1 = (DWORD)d[0];
    pGUID->Data2 = (WORD)d[1];
    pGUID->Data3 = (WORD)d[2];
    pGUID->Data4[0] = (BYTE)d[3];
    pGUID->Data4[1] = (BYTE)d[4];
    pGUID->Data4[2] = (BYTE)d[5];
    pGUID->Data4[3] = (BYTE)d[6];
    pGUID->Data4[4] = (BYTE)d[7];
    pGUID->Data4[5] = (BYTE)d[8];
    pGUID->Data4[6] = (BYTE)d[9];
    pGUID->Data4[7] = (BYTE)d[10];
}

void GuidToText(GUID *pGUID, char *str, int nStrLen)
{
    // note: nStrLen should be set to sizeof(str).
    if (!str) return;
    if (!nStrLen) return;
    str[0] = 0;
    if (!pGUID) return;
    
    DWORD d[11];
    d[0]  = (DWORD)pGUID->Data1;
    d[1]  = (DWORD)pGUID->Data2;
    d[2]  = (DWORD)pGUID->Data3;
    d[3]  = (DWORD)pGUID->Data4[0];
    d[4]  = (DWORD)pGUID->Data4[1];
    d[5]  = (DWORD)pGUID->Data4[2];
    d[6]  = (DWORD)pGUID->Data4[3];
    d[7]  = (DWORD)pGUID->Data4[4];
    d[8]  = (DWORD)pGUID->Data4[5];
    d[9]  = (DWORD)pGUID->Data4[6];
    d[10] = (DWORD)pGUID->Data4[7];

    sprintf(str, "%08X %04X %04X %02X %02X %02X %02X %02X %02X %02X %02X", 
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10]);
}


#ifdef _DEBUG
    void OutputDebugMessage(char *szStartText, HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
    {
        // note: this function does NOT log WM_MOUSEMOVE, WM_NCHITTEST, or WM_SETCURSOR
        //        messages, since they are so frequent.
        // note: these identifiers were pulled from winuser.h

        //if (msg == WM_MOUSEMOVE || msg == WM_NCHITTEST || msg == WM_SETCURSOR)
        //    return;

        #ifdef _DEBUG
            char buf[64];
            int matched = 1;

            sprintf(buf, "WM_");
    
            switch(msg)
            {
            case 0x0001: lstrcat(buf, "CREATE"); break;                      
            case 0x0002: lstrcat(buf, "DESTROY"); break;
            case 0x0003: lstrcat(buf, "MOVE"); break;
            case 0x0005: lstrcat(buf, "SIZE"); break;
            case 0x0006: lstrcat(buf, "ACTIVATE"); break;
            case 0x0007: lstrcat(buf, "SETFOCUS"); break;
            case 0x0008: lstrcat(buf, "KILLFOCUS"); break;
            case 0x000A: lstrcat(buf, "ENABLE"); break;
            case 0x000B: lstrcat(buf, "SETREDRAW"); break;
            case 0x000C: lstrcat(buf, "SETTEXT"); break;
            case 0x000D: lstrcat(buf, "GETTEXT"); break;
            case 0x000E: lstrcat(buf, "GETTEXTLENGTH"); break;
            case 0x000F: lstrcat(buf, "PAINT"); break;
            case 0x0010: lstrcat(buf, "CLOSE"); break;
            case 0x0011: lstrcat(buf, "QUERYENDSESSION"); break;
            case 0x0012: lstrcat(buf, "QUIT"); break;
            case 0x0013: lstrcat(buf, "QUERYOPEN"); break;
            case 0x0014: lstrcat(buf, "ERASEBKGND"); break;
            case 0x0015: lstrcat(buf, "SYSCOLORCHANGE"); break;
            case 0x0016: lstrcat(buf, "ENDSESSION"); break;
            case 0x0018: lstrcat(buf, "SHOWWINDOW"); break;
            case 0x001A: lstrcat(buf, "WININICHANGE"); break;
            case 0x001B: lstrcat(buf, "DEVMODECHANGE"); break;
            case 0x001C: lstrcat(buf, "ACTIVATEAPP"); break;
            case 0x001D: lstrcat(buf, "FONTCHANGE"); break;
            case 0x001E: lstrcat(buf, "TIMECHANGE"); break;
            case 0x001F: lstrcat(buf, "CANCELMODE"); break;
            case 0x0020: lstrcat(buf, "SETCURSOR"); break;
            case 0x0021: lstrcat(buf, "MOUSEACTIVATE"); break;
            case 0x0022: lstrcat(buf, "CHILDACTIVATE"); break;
            case 0x0023: lstrcat(buf, "QUEUESYNC"); break;
            case 0x0024: lstrcat(buf, "GETMINMAXINFO"); break;
            case 0x0026: lstrcat(buf, "PAINTICON"); break;
            case 0x0027: lstrcat(buf, "ICONERASEBKGND"); break;
            case 0x0028: lstrcat(buf, "NEXTDLGCTL"); break;
            case 0x002A: lstrcat(buf, "SPOOLERSTATUS"); break;
            case 0x002B: lstrcat(buf, "DRAWITEM"); break;
            case 0x002C: lstrcat(buf, "MEASUREITEM"); break;
            case 0x002D: lstrcat(buf, "DELETEITEM"); break;
            case 0x002E: lstrcat(buf, "VKEYTOITEM"); break;
            case 0x002F: lstrcat(buf, "CHARTOITEM"); break;
            case 0x0030: lstrcat(buf, "SETFONT"); break;
            case 0x0031: lstrcat(buf, "GETFONT"); break;
            case 0x0032: lstrcat(buf, "SETHOTKEY"); break;
            case 0x0033: lstrcat(buf, "GETHOTKEY"); break;
            case 0x0037: lstrcat(buf, "QUERYDRAGICON"); break;
            case 0x0039: lstrcat(buf, "COMPAREITEM"); break;
            case 0x0041: lstrcat(buf, "COMPACTING"); break;
            case 0x0044: lstrcat(buf, "COMMNOTIFY"); break;
            case 0x0046: lstrcat(buf, "WINDOWPOSCHANGING"); break;
            case 0x0047: lstrcat(buf, "WINDOWPOSCHANGED"); break;
            case 0x0048: lstrcat(buf, "POWER"); break;
            case 0x004A: lstrcat(buf, "COPYDATA"); break;
            case 0x004B: lstrcat(buf, "CANCELJOURNAL"); break;

            #if(WINVER >= 0x0400)
            case 0x004E: lstrcat(buf, "NOTIFY"); break;
            case 0x0050: lstrcat(buf, "INPUTLANGCHANGEREQUEST"); break;
            case 0x0051: lstrcat(buf, "INPUTLANGCHANGE"); break;
            case 0x0052: lstrcat(buf, "TCARD"); break;
            case 0x0053: lstrcat(buf, "HELP"); break;
            case 0x0054: lstrcat(buf, "USERCHANGED"); break;
            case 0x0055: lstrcat(buf, "NOTIFYFORMAT"); break;
            case 0x007B: lstrcat(buf, "CONTEXTMENU"); break;
            case 0x007C: lstrcat(buf, "STYLECHANGING"); break;
            case 0x007D: lstrcat(buf, "STYLECHANGED"); break;
            case 0x007E: lstrcat(buf, "DISPLAYCHANGE"); break;
            case 0x007F: lstrcat(buf, "GETICON"); break;
            case 0x0080: lstrcat(buf, "SETICON"); break;
            #endif 

            case 0x0081: lstrcat(buf, "NCCREATE"); break;
            case 0x0082: lstrcat(buf, "NCDESTROY"); break;
            case 0x0083: lstrcat(buf, "NCCALCSIZE"); break;
            case 0x0084: lstrcat(buf, "NCHITTEST"); break;
            case 0x0085: lstrcat(buf, "NCPAINT"); break;
            case 0x0086: lstrcat(buf, "NCACTIVATE"); break;
            case 0x0087: lstrcat(buf, "GETDLGCODE"); break;
            case 0x0088: lstrcat(buf, "SYNCPAINT"); break;
            case 0x00A0: lstrcat(buf, "NCMOUSEMOVE"); break;
            case 0x00A1: lstrcat(buf, "NCLBUTTONDOWN"); break;
            case 0x00A2: lstrcat(buf, "NCLBUTTONUP"); break;
            case 0x00A3: lstrcat(buf, "NCLBUTTONDBLCLK"); break;
            case 0x00A4: lstrcat(buf, "NCRBUTTONDOWN"); break;
            case 0x00A5: lstrcat(buf, "NCRBUTTONUP"); break;
            case 0x00A6: lstrcat(buf, "NCRBUTTONDBLCLK"); break;
            case 0x00A7: lstrcat(buf, "NCMBUTTONDOWN"); break;
            case 0x00A8: lstrcat(buf, "NCMBUTTONUP"); break;
            case 0x00A9: lstrcat(buf, "NCMBUTTONDBLCLK"); break;
            case 0x0100: lstrcat(buf, "KEYDOWN"); break;
            case 0x0101: lstrcat(buf, "KEYUP"); break;
            case 0x0102: lstrcat(buf, "CHAR"); break;
            case 0x0103: lstrcat(buf, "DEADCHAR"); break;
            case 0x0104: lstrcat(buf, "SYSKEYDOWN"); break;
            case 0x0105: lstrcat(buf, "SYSKEYUP"); break;
            case 0x0106: lstrcat(buf, "SYSCHAR"); break;
            case 0x0107: lstrcat(buf, "SYSDEADCHAR"); break;
            case 0x0108: lstrcat(buf, "KEYLAST"); break;

            #if(WINVER >= 0x0400)
            case 0x010D: lstrcat(buf, "IME_STARTCOMPOSITION"); break;
            case 0x010E: lstrcat(buf, "IME_ENDCOMPOSITION"); break;
            case 0x010F: lstrcat(buf, "IME_COMPOSITION"); break;
            //case 0x010F: lstrcat(buf, "IME_KEYLAST"); break;
            #endif 

            case 0x0110: lstrcat(buf, "INITDIALOG"); break;
            case 0x0111: lstrcat(buf, "COMMAND"); break;
            case 0x0112: lstrcat(buf, "SYSCOMMAND"); break;
            case 0x0113: lstrcat(buf, "TIMER"); break;
            case 0x0114: lstrcat(buf, "HSCROLL"); break;
            case 0x0115: lstrcat(buf, "VSCROLL"); break;
            case 0x0116: lstrcat(buf, "INITMENU"); break;
            case 0x0117: lstrcat(buf, "INITMENUPOPUP"); break;
            case 0x011F: lstrcat(buf, "MENUSELECT"); break;
            case 0x0120: lstrcat(buf, "MENUCHAR"); break;
            case 0x0121: lstrcat(buf, "ENTERIDLE"); break;
            #if(WINVER >= 0x0500)
            case 0x0122: lstrcat(buf, "MENURBUTTONUP"); break;
            case 0x0123: lstrcat(buf, "MENUDRAG"); break;
            case 0x0124: lstrcat(buf, "MENUGETOBJECT"); break;
            case 0x0125: lstrcat(buf, "UNINITMENUPOPUP"); break;
            case 0x0126: lstrcat(buf, "MENUCOMMAND"); break;
            #endif 

            case 0x0132: lstrcat(buf, "CTLCOLORMSGBOX"); break;
            case 0x0133: lstrcat(buf, "CTLCOLOREDIT"); break;
            case 0x0134: lstrcat(buf, "CTLCOLORLISTBOX"); break;
            case 0x0135: lstrcat(buf, "CTLCOLORBTN"); break;
            case 0x0136: lstrcat(buf, "CTLCOLORDLG"); break;
            case 0x0137: lstrcat(buf, "CTLCOLORSCROLLBAR"); break;
            case 0x0138: lstrcat(buf, "CTLCOLORSTATIC"); break;

            //case 0x0200: lstrcat(buf, "MOUSEFIRST"); break;
            case 0x0200: lstrcat(buf, "MOUSEMOVE"); break;
            case 0x0201: lstrcat(buf, "LBUTTONDOWN"); break;
            case 0x0202: lstrcat(buf, "LBUTTONUP"); break;
            case 0x0203: lstrcat(buf, "LBUTTONDBLCLK"); break;
            case 0x0204: lstrcat(buf, "RBUTTONDOWN"); break;
            case 0x0205: lstrcat(buf, "RBUTTONUP"); break;
            case 0x0206: lstrcat(buf, "RBUTTONDBLCLK"); break;
            case 0x0207: lstrcat(buf, "MBUTTONDOWN"); break;
            case 0x0208: lstrcat(buf, "MBUTTONUP"); break;
            case 0x0209: lstrcat(buf, "MBUTTONDBLCLK"); break;

            #if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
            case 0x020A: lstrcat(buf, "MOUSEWHEEL"); break;
            case 0x020E: lstrcat(buf, "MOUSELAST"); break;
            #else
            //case 0x0209: lstrcat(buf, "MOUSELAST"); break;
            #endif 

            case 0x0210: lstrcat(buf, "PARENTNOTIFY"); break;
            case 0x0211: lstrcat(buf, "ENTERMENULOOP"); break;
            case 0x0212: lstrcat(buf, "EXITMENULOOP"); break;

            #if(WINVER >= 0x0400)
            case 0x0213: lstrcat(buf, "NEXTMENU"); break;
            case 0x0214: lstrcat(buf, "SIZING"); break;
            case 0x0215: lstrcat(buf, "CAPTURECHANGED"); break;
            case 0x0216: lstrcat(buf, "MOVING"); break;
            case 0x0218: lstrcat(buf, "POWERBROADCAST"); break;
            case 0x0219: lstrcat(buf, "DEVICECHANGE"); break;
            #endif 

            /*
            case 0x0220: lstrcat(buf, "MDICREATE"); break;
            case 0x0221: lstrcat(buf, "MDIDESTROY"); break;
            case 0x0222: lstrcat(buf, "MDIACTIVATE"); break;
            case 0x0223: lstrcat(buf, "MDIRESTORE"); break;
            case 0x0224: lstrcat(buf, "MDINEXT"); break;
            case 0x0225: lstrcat(buf, "MDIMAXIMIZE"); break;
            case 0x0226: lstrcat(buf, "MDITILE"); break;
            case 0x0227: lstrcat(buf, "MDICASCADE"); break;
            case 0x0228: lstrcat(buf, "MDIICONARRANGE"); break;
            case 0x0229: lstrcat(buf, "MDIGETACTIVE"); break;
            */

            case 0x0230: lstrcat(buf, "MDISETMENU"); break;
            case 0x0231: lstrcat(buf, "ENTERSIZEMOVE"); break;
            case 0x0232: lstrcat(buf, "EXITSIZEMOVE"); break;
            case 0x0233: lstrcat(buf, "DROPFILES"); break;
            case 0x0234: lstrcat(buf, "MDIREFRESHMENU"); break;


            /*
            #if(WINVER >= 0x0400)
            case 0x0281: lstrcat(buf, "IME_SETCONTEXT"); break;
            case 0x0282: lstrcat(buf, "IME_NOTIFY"); break;
            case 0x0283: lstrcat(buf, "IME_CONTROL"); break;
            case 0x0284: lstrcat(buf, "IME_COMPOSITIONFULL"); break;
            case 0x0285: lstrcat(buf, "IME_SELECT"); break;
            case 0x0286: lstrcat(buf, "IME_CHAR"); break;
            #endif 
            #if(WINVER >= 0x0500)
            case 0x0288: lstrcat(buf, "IME_REQUEST"); break;
            #endif 
            #if(WINVER >= 0x0400)
            case 0x0290: lstrcat(buf, "IME_KEYDOWN"); break;
            case 0x0291: lstrcat(buf, "IME_KEYUP"); break;
            #endif 
            */

            #if(_WIN32_WINNT >= 0x0400)
            case 0x02A1: lstrcat(buf, "MOUSEHOVER"); break;
            case 0x02A3: lstrcat(buf, "MOUSELEAVE"); break;
            #endif 

            case 0x0300: lstrcat(buf, "CUT"); break;
            case 0x0301: lstrcat(buf, "COPY"); break;
            case 0x0302: lstrcat(buf, "PASTE"); break;
            case 0x0303: lstrcat(buf, "CLEAR"); break;
            case 0x0304: lstrcat(buf, "UNDO"); break;
            case 0x0305: lstrcat(buf, "RENDERFORMAT"); break;
            case 0x0306: lstrcat(buf, "RENDERALLFORMATS"); break;
            case 0x0307: lstrcat(buf, "DESTROYCLIPBOARD"); break;
            case 0x0308: lstrcat(buf, "DRAWCLIPBOARD"); break;
            case 0x0309: lstrcat(buf, "PAINTCLIPBOARD"); break;
            case 0x030A: lstrcat(buf, "VSCROLLCLIPBOARD"); break;
            case 0x030B: lstrcat(buf, "SIZECLIPBOARD"); break;
            case 0x030C: lstrcat(buf, "ASKCBFORMATNAME"); break;
            case 0x030D: lstrcat(buf, "CHANGECBCHAIN"); break;
            case 0x030E: lstrcat(buf, "HSCROLLCLIPBOARD"); break;
            case 0x030F: lstrcat(buf, "QUERYNEWPALETTE"); break;
            case 0x0310: lstrcat(buf, "PALETTEISCHANGING"); break;
            case 0x0311: lstrcat(buf, "PALETTECHANGED"); break;
            case 0x0312: lstrcat(buf, "HOTKEY"); break;

            #if(WINVER >= 0x0400)
            case 0x0317: lstrcat(buf, "PRINT"); break;
            case 0x0318: lstrcat(buf, "PRINTCLIENT"); break;

            case 0x0358: lstrcat(buf, "HANDHELDFIRST"); break;
            case 0x035F: lstrcat(buf, "HANDHELDLAST"); break;

            case 0x0360: lstrcat(buf, "AFXFIRST"); break;
            case 0x037F: lstrcat(buf, "AFXLAST"); break;
            #endif 

            case 0x0380: lstrcat(buf, "PENWINFIRST"); break;
            case 0x038F: lstrcat(buf, "PENWINLAST"); break;

            default: 
                sprintf(buf, "unknown"); 
                matched = 0;
                break;
            }

            int n = strlen(buf);
            int desired_len = 24;
            int spaces_to_append = desired_len-n;
            if (spaces_to_append>0)
            {
                for (int i=0; i<spaces_to_append; i++)
                    buf[n+i] = ' ';
                buf[desired_len] = 0;
            }      

            char buf2[256];
            if (matched)
                sprintf(buf2, "%shwnd=%08x, msg=%s, w=%08x, l=%08x\n", szStartText, hwnd, buf, wParam, lParam);
            else
                sprintf(buf2, "%shwnd=%08x, msg=unknown/0x%08x, w=%08x, l=%08x\n", szStartText, hwnd, msg, wParam, lParam);
            OutputDebugString(buf2);
        #endif
    }
#endif

void DownloadDirectX(HWND hwnd)
{
}

void MissingDirectX(HWND hwnd)
{
}

bool CheckForMMX()
{
    DWORD bMMX = 0;
    DWORD *pbMMX = &bMMX;
    __try {
        __asm {
            mov eax, 1
            cpuid
            mov edi, pbMMX
            mov dword ptr [edi], edx
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        bMMX = 0;
    }

    if (bMMX & 0x00800000)  // check bit 23
		return true;

	return false;
}

bool CheckForSSE()
{
#ifdef _WIN64
	return true; // All x64 processors support SSE
#else
    /*
    The SSE instruction set was introduced with the Pentium III and features:
        * Additional MMX instructions such as min/max
        * Prefetch and write-through instructions for optimizing data movement 
            from and to the L2/L3 caches and main memory
        * 8 New 128 bit XMM registers (xmm0..xmm7) and corresponding 32 bit floating point 
            (single precision) instructions
    */

	DWORD bSSE = 0;
	DWORD *pbSSE = &bSSE;
    __try {
	    __asm
	    {
		    mov eax, 1
		    cpuid
            mov edi, pbSSE
            mov dword ptr [edi], edx
	    }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        bSSE = 0;
    }

	if (bSSE & 0x02000000)  // check bit 25
		return true;

	return false;
#endif
}


//----------------------------------------------------------------------


LRESULT GetWinampVersion(HWND winamp)
{
	static LRESULT version=0;
	if (!version)
		version=SendMessage(winamp,WM_WA_IPC,0,0);
	return version;
}


void GetDirectoryFiles(const wchar_t *dir, const wchar_t *pattern, bool recurse, std::vector<std::wstring> &files)
{
	WIN32_FIND_DATAW fd;
    ZeroMemory(&fd, sizeof(fd));

	wchar_t mask[MAX_PATH];
	swprintf(mask, L"%s\\%s", dir, pattern);  

	HANDLE hFind = FindFirstFileW(mask, &fd );
	if (hFind == INVALID_HANDLE_VALUE) 
	{
		return;
	}

	do
	{
		if (fd.cFileName[0] == '.') 
			continue;

		std::wstring path = std::wstring(dir) + L"\\" + std::wstring(fd.cFileName);

		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if (recurse)
			{
				GetDirectoryFiles(path.c_str(), pattern, true, files);
			}
			continue;
		}

		files.push_back(path);
	} while (FindNextFileW(hFind, &fd));

	FindClose(hFind);
}
