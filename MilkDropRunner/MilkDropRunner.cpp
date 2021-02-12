// MilkDropRunner.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <stdio.h>
#include "MilkDropRunner.h"
#define USE_VIS_HDR_HWND
#include "../src/vis_milk2/VIS.H"



#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name


winampVisModule *g_vismodule;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);


bool LoadVisPlugin(HWND hWnd, const char *path)
{
	HINSTANCE hPluginInstance = LoadLibrary(path);
	if (!hPluginInstance) 
	{
		return false;
	}

	winampVisGetHeaderType visGetHeaderProc = (winampVisGetHeaderType )GetProcAddress(hPluginInstance, "winampVisGetHeader");
	if (!visGetHeaderProc) 
	{
		return false;
	}

	winampVisHeader* header = visGetHeaderProc(hWnd);
	if (!header) 
	{
		return false;
	}

	winampVisModule *module = header->getModule(0);
	if (!module)
	{
		return false;
	}

	module->hwndParent = hWnd;
	module->hDllInstance = hPluginInstance;
	module->nCh = 2;
	module->sRate = 44100;
	module->spectrumNch = 0;
	module->waveformNch = 2;

	//initialize module
	int result = module->Init(module);
	if (result != 0)
	{
		return false;
	}

	g_vismodule = module;
	return true;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_MILKDROPRUNNER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

   HWND hWnd;
   hInst = hInstance; // Store instance handle in our global variable
   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

 	if (!LoadVisPlugin(hWnd, "vis_milk2.dll"))
	{
		return FALSE;
	}

	//char dir[MAX_PATH];
	//GetCurrentDirectoryA(sizeof(dir), dir);

	FILE *readFile = NULL;
	fopen_s(&readFile, "audio.raw", "rb");

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MILKDROPRUNNER));

	while(TRUE)
	{
		// Check to see if any messages are waiting in the queue
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Translate the message and dispatch it to WindowProc()
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// If the message is WM_QUIT, exit the while loop
		if(msg.message == WM_QUIT)
			break;

		if (g_vismodule)
		{
			if (readFile)
			{
				// read audio from source file
				if (fread(g_vismodule->waveformData, sizeof(g_vismodule->waveformData), 1, readFile) != 1) {
					fseek(readFile, 0, SEEK_SET);
				}
			}


			if (g_vismodule->Render(g_vismodule))
			{
				// stop rendering...
				g_vismodule = NULL;
				break;
			}
		}
			
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MILKDROPRUNNER));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_MILKDROPRUNNER);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}




//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
