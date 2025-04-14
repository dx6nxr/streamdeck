#ifndef WASAPI_CONTROLLER_H
#define WASAPI_CONTROLLER_H

#include <windows.h>
#include <vector>
#include <string>

//moved these globals to .cpp
//extern HINSTANCE hInst;
//extern HWND g_hwnd;

// Constants
#define WM_USER_TRAYICON (WM_USER + 1)

// Function declarations
void AddTrayIcon(HWND hwnd, HINSTANCE hinstance, LPCWSTR tip);
void RemoveTrayIcon(HWND hwnd);
void HandleTrayIconClick(HWND hwnd, LPARAM lParam);
std::string ws2s(const std::wstring& wstr);

// WASAPI Functions
bool InitializeWasapi();
void SetApplicationVolume(const std::wstring& appName, float volume);
std::vector<std::wstring> GetApplicationNames();
void ToggleMuteApplication(const std::wstring& appName);
void ShowTrayBalloonTip(const wchar_t* title, const wchar_t* message, DWORD infoFlags);
extern void RefreshAudioSessions();

bool g_wasapiInitialized = false;


#endif // WASAPI_CONTROLLER_H