#include "framework.h"
#include "streamdeck-wasapi.h" // Include the header
#include "resource.h"       // For the icon resource
#include <iostream>
#include <vector>
#include <shellapi.h> // Add this include to define NIIF_INFO


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND g_hwnd;
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
//VolumeMixer g_volumeMixer;                       // Global VolumeMixer object  NO VolumeMixer

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Included from wasapi_controller.cpp
extern void AddTrayIcon(HWND hwnd, HINSTANCE hinstance, LPCWSTR tip);
extern void RemoveTrayIcon(HWND hwnd);
extern void HandleTrayIconClick(HWND hwnd, LPARAM lParam);
extern bool InitializeWasapi();
extern void SetApplicationVolume(const std::wstring& appName, float volume);
extern std::vector<std::wstring> GetApplicationNames();
extern void ToggleMuteApplication(const std::wstring& appName);
extern void RefreshAudioSessions();
extern void ShowTrayBalloonTip(const wchar_t* title, const wchar_t* message, DWORD infoFlags);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_STREAMDECKWASAPI, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_STREAMDECKWASAPI));
    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION)); // Use default icon.  Make sure this is in resource.h
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL; // No menu
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    // Create a console window for debug output
    AllocConsole();
    // Redirect stdout to the console
    FILE* pConsole;
    freopen_s(&pConsole, "CONOUT$", "w", stdout);
    freopen_s(&pConsole, "CONOUT$", "w", stderr);

    // Set up console for wide character output (for wcout)
    FILE* pConsoleW;
    _wfreopen_s(&pConsoleW, L"CONOUT$", L"w", stdout);
    std::wcout.clear();
    std::cout.clear();

    g_hwnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!g_hwnd)
    {
        return FALSE;
    }

    // Add the tray icon - this is critical
    AddTrayIcon(g_hwnd, hInstance, L"streamdeck\nRight click for menu, Left click for info.");

    // Don't show the window at all for a tray-only app
    // ShowWindow(g_hwnd, nCmdShow); // Comment this out or replace with SW_HIDE
    // UpdateWindow(g_hwnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Initialize the WASAPI
        if (!InitializeWasapi()) {
            std::cerr << "WASAPI initialization failed" << std::endl;
            PostQuitMessage(1);
            return -1;
        }
        break;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        if (wmId == IDM_EXIT) { // Exit
            DestroyWindow(hWnd);
        }
        else if (wmId == IDM_ABOUT) { // About
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        }
        else if (wmId == IDM_REFRESH) { // Refresh Audio Sessions
            RefreshAudioSessions();
            ShowTrayBalloonTip(L"Sessions Refreshed", L"Audio sessions have been refreshed.", NIIF_INFO);
        }
        else if (wmId >= IDM_APP_BASE) { // Application volume control
            int appIndex = wmId - IDM_APP_BASE;
            std::vector<std::wstring> appNames = GetApplicationNames();
            if (appIndex >= 0 && appIndex < static_cast<int>(appNames.size())) {
                ToggleMuteApplication(appNames[appIndex]);
            } 
        }
        else if (wmId == IDM_SETTINGS) {
            // do nothing for now. Settings will be added later
        }
        else {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        RemoveTrayIcon(hWnd);
        PostQuitMessage(0);
        break;
    case WM_USER_TRAYICON: // Our custom message for tray icon events
        HandleTrayIconClick(hWnd, lParam);
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