#ifndef STREAMDECK_WASAPI_H
#define STREAMDECK_WASAPI_H

#include <windows.h>
// No need to include VolumeMixer.h
// #include "VolumeMixer.h"

// Forward declarations
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
HWND                CreateAppWindow(HINSTANCE hinstance, LPCWSTR className, LPCWSTR windowName);

// Global variables
extern HINSTANCE hInst;
extern HWND g_hwnd;
//extern VolumeMixer g_volumeMixer;  // No VolumeMixer

// Constants
#define WM_USER_TRAYICON (WM_USER + 1)

#endif // STREAMDECK_WASAPI_H