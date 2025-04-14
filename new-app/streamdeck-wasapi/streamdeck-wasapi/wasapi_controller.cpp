#include "framework.h"
#include "streamdeck-wasapi.h"
#include "wasapi_controller.h"
#include <iostream>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>
#include <shellapi.h>
#include <windows.h>
#include <audiopolicy.h>
#include <fstream>
#include <algorithm> // For std::transform
#include "Resource.h"
#include <cwctype>


// Global variables
extern HINSTANCE hInst; // Get the instance from main
extern HWND g_hwnd;
NOTIFYICONDATAW g_nid;

// Function to convert wide string to narrow string
// Function to convert wide string to narrow string
std::string ws2s(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) {
        throw std::runtime_error("WideCharToMultiByte failed");
    }

    std::string result(sizeNeeded - 1, '\0'); // Exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], sizeNeeded, nullptr, nullptr);
    return result;
}


// WASAPI Global Variables
IMMDeviceEnumerator* g_pEnumerator = nullptr;
IMMDevice* g_pDevice = nullptr;
IAudioSessionManager2* g_pSessionManager = nullptr;
std::vector<ISimpleAudioVolume*> g_sessionVolumes;
std::vector<std::wstring> g_sessionNames;


void ShowTrayBalloonTip(const wchar_t* title, const wchar_t* message, DWORD infoFlags = NIIF_INFO) {
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = infoFlags; // Can be NIIF_INFO, NIIF_WARNING, NIIF_ERROR
    wcscpy_s(g_nid.szInfoTitle, title);
    wcscpy_s(g_nid.szInfo, message);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
// Function to add the tray icon
void AddTrayIcon(HWND hwnd, HINSTANCE hinstance, LPCWSTR tip) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_USER_TRAYICON;

    // Use your custom icon instead of the default one
    g_nid.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_STREAMDECKWASAPI));
    if (g_nid.hIcon == NULL) {
        // Fall back to default icon if custom one fails
        g_nid.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_APPLICATION));
        std::cerr << "LoadIcon failed for custom icon, using default" << std::endl;
    }

    wcscpy_s(g_nid.szTip, tip);
    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        std::cerr << "Shell_NotifyIconW(NIM_ADD) failed" << std::endl;
        OutputDebugStringW(L"Failed to add tray icon\n");
    }
    else {
        OutputDebugStringW(L"Tray icon added successfully\n");
    }

    // Add this to your InitInstance function
    ShowTrayBalloonTip(L"WASAPI Volume Control", L"The application is now running in the system tray.", NIIF_INFO);

}

// Function to remove the tray icon
void RemoveTrayIcon(HWND hwnd) {
    if (!Shell_NotifyIconW(NIM_DELETE, &g_nid)) {
        std::cerr << "Shell_NotifyIconW(NIM_DELETE) failed" << std::endl;
        // Handle error
    }
}

// Add this function to create a menu with icons
void ShowTrayIconMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Get applications
    std::vector<std::wstring> appNames = GetApplicationNames();

    // Add application entries
    for (size_t i = 0; i < appNames.size(); ++i) {
        if (appNames[i].empty() || appNames[i] == L"Unknown Session") {
            continue;
        }
        AppendMenuW(hMenu, MF_STRING, IDM_APP_BASE + i, appNames[i].c_str());
    }

    // Add a separator and refresh button
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_REFRESH, L"Refresh Audio Sessions");
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Settings");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"About");
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    // Ensure the menu is dismissed when clicked outside
    SetForegroundWindow(hwnd);

    // Display the menu
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);

    // Post a dummy message to ensure the menu is dismissed properly
    PostMessage(hwnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}


// Function to handle tray icon clicks
void HandleTrayIconClick(HWND hwnd, LPARAM lParam) {
    OutputDebugStringW(L"Tray icon clicked\n");

    switch (lParam) {
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        ShowTrayIconMenu(hwnd, pt);
        break;
    }
    default:
        break;
    }
}

// Function to initialize WASAPI
bool InitializeWasapi() {
    std::cerr << "\n\n*** Initializing WASAPI ***\n\n" << std::endl;
    
    // If already initialized, clean up first
    if (g_wasapiInitialized) {
        std::cerr << "WASAPI already initialized, cleaning up for re-initialization..." << std::endl;
        
        // Clean up existing session volumes
        for (size_t i = 0; i < g_sessionVolumes.size(); ++i) {
            if (g_sessionVolumes[i]) {
                g_sessionVolumes[i]->Release();
                g_sessionVolumes[i] = nullptr;
            }
        }
        
        // Release other resources
        if (g_pSessionManager) {
            g_pSessionManager->Release();
            g_pSessionManager = nullptr;
        }
        
        if (g_pDevice) {
            g_pDevice->Release();
            g_pDevice = nullptr;
        }
        
        if (g_pEnumerator) {
            g_pEnumerator->Release();
            g_pEnumerator = nullptr;
        }
        
        g_sessionVolumes.clear();
        g_sessionNames.clear();
        g_wasapiInitialized = false;
    }

    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        std::cerr << "CoInitialize failed: " << std::hex << hr << std::endl;
        return false;
    }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&g_pEnumerator);
    if (FAILED(hr)) {
        std::cerr << "CoCreateInstance(MMDeviceEnumerator) failed: " << std::hex << hr << std::endl;
        CoUninitialize();
        return false;
    }

    hr = g_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &g_pDevice);
    if (FAILED(hr)) {
        std::cerr << "GetDefaultAudioEndpoint failed: " << std::hex << hr << std::endl;
        g_pEnumerator->Release();
        CoUninitialize();
        return false;
    }

    hr = g_pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&g_pSessionManager);
    if (FAILED(hr)) {
        std::cerr << "Activate(IAudioSessionManager2) failed: " << std::hex << hr << std::endl;
        g_pDevice->Release();
        g_pEnumerator->Release();
        CoUninitialize();
        return false;
    }

    IAudioSessionEnumerator* pSessionEnumerator = NULL;
    hr = g_pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
    if (FAILED(hr)) {
        std::cerr << "GetSessionEnumerator failed: " << std::hex << hr << std::endl;
        g_pSessionManager->Release();
        g_pDevice->Release();
        g_pEnumerator->Release();
        CoUninitialize();
        return false;
    }

    int sessionCount;
    hr = pSessionEnumerator->GetCount(&sessionCount);
    if (FAILED(hr)) {
        std::cerr << "GetCount failed: " << std::hex << hr << std::endl;
        pSessionEnumerator->Release();
        g_pSessionManager->Release();
        g_pDevice->Release();
        g_pEnumerator->Release();
        CoUninitialize();
        return false;
    }

    std::cerr << "Found " << sessionCount << " audio sessions" << std::endl;
    g_sessionVolumes.resize(sessionCount);
    g_sessionNames.resize(sessionCount);

    for (int i = 0; i < sessionCount; ++i) {
        IAudioSessionControl* pSessionControl = NULL;
        hr = pSessionEnumerator->GetSession(i, &pSessionControl);
        if (FAILED(hr)) {
            g_sessionVolumes[i] = NULL; // Set this slot to NULL since we're skipping it
            g_sessionNames[i] = L"Unknown Session";
            continue;
        }

        // FIXED: Don't create an unused local variable, just use g_sessionVolumes[i] directly
        hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&g_sessionVolumes[i]);
        if (FAILED(hr)) {
            g_sessionVolumes[i] = NULL; // Set to NULL explicitly on failure
            pSessionControl->Release();
            g_sessionNames[i] = L"Unknown Session";
            continue;
        }

        IAudioSessionControl2* pSessionControl2 = NULL;
        hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
        if (SUCCEEDED(hr)) {
            DWORD processId = 0;
            hr = pSessionControl2->GetProcessId(&processId);

            LPWSTR sessionDisplayName = NULL;
            if (SUCCEEDED(pSessionControl2->GetDisplayName(&sessionDisplayName))) {
                std::cerr << "Session " << i << " display name: " << ws2s(sessionDisplayName) << std::endl;
                CoTaskMemFree(sessionDisplayName);
            }

            std::wstring appName = L"Unknown Session";
            if (SUCCEEDED(hr) && processId != 0) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
                if (hProcess != NULL) {
                    WCHAR processName[MAX_PATH];
                    DWORD size = MAX_PATH;
                    if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
                        WCHAR* fileName = wcsrchr(processName, L'\\');
                        if (fileName) {
                            appName = fileName + 1;
                        }
                        else {
                            appName = processName;
                        }
                        std::cerr << "Session " << i << " process name: " << ws2s(appName) << std::endl;
                    }
                    CloseHandle(hProcess);
                }
            }
            g_sessionNames[i] = appName;
            pSessionControl2->Release();
        }
        else {
            g_sessionNames[i] = L"Unknown Session";
        }

        // REMOVED: Don't overwrite g_sessionVolumes[i] with pSimpleVolume
        // g_sessionVolumes[i] = pSimpleVolume; 

        pSessionControl->Release();
    }

    pSessionEnumerator->Release();
    g_wasapiInitialized = true;

    // Debug output of session names
    std::cerr << "Audio session names:" << std::endl;
    for (const auto& name : g_sessionNames) {
        std::cerr << "  " << ws2s(name) << std::endl;
    }

    return true;
}

void RefreshAudioSessions() {
    std::cerr << "\n\n*** Refreshing audio sessions... ***\n\n" << std::endl;

    // Clean up existing session volumes
    for (size_t i = 0; i < g_sessionVolumes.size(); ++i) {
        if (g_sessionVolumes[i]) {
            g_sessionVolumes[i]->Release();
            g_sessionVolumes[i] = nullptr;
        }
    }

    // Clear the vectors
    g_sessionVolumes.clear();
    g_sessionNames.clear();

    // Set as not initialized so the next call to GetApplicationNames will reinitialize
    g_wasapiInitialized = false;

    // Re-initialize the sessions
    if (!InitializeWasapi()) {
        std::cerr << "Failed to reinitialize WASAPI during refresh." << std::endl;
    }
    else {
        std::cerr << "Audio sessions refreshed successfully." << std::endl;
        
        // Output all current session names
        std::cerr << "Current audio sessions after refresh:" << std::endl;
        for (size_t i = 0; i < g_sessionNames.size(); ++i) {
            std::cerr << "  " << i << ": " << ws2s(g_sessionNames[i]) << std::endl;
        }
    }
}




void SetApplicationVolume(const std::wstring& appName, float volume) {
    std::wcout << L"\n======= Setting volume for app: \"" << appName << L"\" to " << volume << L" ========" << std::endl;
    
    if (!g_wasapiInitialized) {
        std::cerr << "WASAPI not initialized, initializing now..." << std::endl;
        if (!InitializeWasapi()) {
            std::cerr << "Failed to initialize WASAPI for volume control" << std::endl;
            return;
        }
    }

    // Debug: print current audio sessions
    std::wcout << L"Current audio sessions (" << g_sessionNames.size() << L"):" << std::endl;
    for (size_t i = 0; i < g_sessionNames.size(); ++i) {
        std::wcout << L"  " << i << L": \"" << g_sessionNames[i] << L"\" (volume control: " 
                  << (g_sessionVolumes[i] ? L"available" : L"NULL") << L")" << std::endl;
    }

    // Convert appName to lowercase for case-insensitive comparison
    std::wstring lowerAppName = appName;
    std::transform(lowerAppName.begin(), lowerAppName.end(), lowerAppName.begin(), 
                   [](wchar_t c) { return std::towlower(c); });

    // Try exact match first (case-insensitive)
    std::wcout << L"Looking for exact match (case-insensitive)..." << std::endl;
    for (size_t i = 0; i < g_sessionNames.size(); ++i) {
        // Convert session name to lowercase for case-insensitive comparison
        std::wstring lowerSessionName = g_sessionNames[i];
        std::transform(lowerSessionName.begin(), lowerSessionName.end(), lowerSessionName.begin(),
                       [](wchar_t c) { return std::towlower(c); });
                       
        if (lowerSessionName == lowerAppName) {
            std::wcout << L"  EXACT MATCH found with session " << i << L": \"" << g_sessionNames[i] << L"\"" << std::endl;
            if (g_sessionVolumes[i]) {
                g_sessionVolumes[i]->SetMasterVolume(volume, NULL);
                std::wcout << L"  Volume of " << g_sessionNames[i] << L" set to " << volume << std::endl;
                return;
            }
            else {
                std::wcout << L"  ERROR: Volume interface is NULL for this session" << std::endl;
            }
        }
    }
    
    // If no exact match, try partial matches (case-insensitive)
    std::wcout << L"No exact match found, trying partial matches (case-insensitive)..." << std::endl;
    
    // First try normal partial match (app name in session name)
    for (size_t i = 0; i < g_sessionNames.size(); ++i) {
        // Convert session name to lowercase for case-insensitive comparison
        std::wstring lowerSessionName = g_sessionNames[i];
        std::transform(lowerSessionName.begin(), lowerSessionName.end(), lowerSessionName.begin(),
                       [](wchar_t c) { return std::towlower(c); });
                       
        // Check if app name is found within session name (case-insensitive)
        bool partialMatch = lowerSessionName.find(lowerAppName) != std::wstring::npos;
        if (partialMatch) {
            std::wcout << L"  PARTIAL MATCH found: \"" << appName << L"\" is part of \"" << g_sessionNames[i] << L"\"" << std::endl;
            if (g_sessionVolumes[i]) {
                g_sessionVolumes[i]->SetMasterVolume(volume, NULL);
                std::wcout << L"  Volume of " << g_sessionNames[i] << L" set to " << volume << std::endl;
                return;
            }
            else {
                std::wcout << L"  ERROR: Volume interface is NULL for this session" << std::endl;
            }
        }
    }
    
    // Then try reverse partial match (session name in app name)
    for (size_t i = 0; i < g_sessionNames.size(); ++i) {
        if (!g_sessionNames[i].empty()) {
            // Convert session name to lowercase for case-insensitive comparison
            std::wstring lowerSessionName = g_sessionNames[i];
            std::transform(lowerSessionName.begin(), lowerSessionName.end(), lowerSessionName.begin(),
                           [](wchar_t c) { return std::towlower(c); });
                           
            bool reverseMatch = lowerAppName.find(lowerSessionName) != std::wstring::npos;
            if (reverseMatch) {
                std::wcout << L"  REVERSE MATCH found: \"" << g_sessionNames[i] << L"\" is part of \"" << appName << L"\"" << std::endl;
                if (g_sessionVolumes[i]) {
                    g_sessionVolumes[i]->SetMasterVolume(volume, NULL);
                    std::wcout << L"  Volume of " << g_sessionNames[i] << L" set to " << volume << std::endl;
                    return;
                }
                else {
                    std::wcout << L"  ERROR: Volume interface is NULL for this session" << std::endl;
                }
            }
        }
    }

    // No match found
    std::wcout << L"⚠️ NO MATCH FOUND for \"" << appName << L"\"" << std::endl;
}

std::vector<std::wstring> GetApplicationNames() {
    if (!g_wasapiInitialized) {
        InitializeWasapi();
    }
    return g_sessionNames;
}


void ToggleMuteApplication(const std::wstring& appName) {
    if (!g_wasapiInitialized) {
        InitializeWasapi();
    }

    for (size_t i = 0; i < g_sessionNames.size(); ++i) {
        if (g_sessionNames[i].find(appName) != std::wstring::npos) {
            //set the volunme of the app to 0
			float currentVolume = 0.0f;
			g_sessionVolumes[i]->GetMasterVolume(&currentVolume);
			if (currentVolume > 0.0f) {
				g_sessionVolumes[i]->SetMasterVolume(0.0f, NULL);
				std::wcout << L"Volume of " << g_sessionNames[i] << L" muted." << std::endl;
			}
            else {
                g_sessionVolumes[i]->SetMasterVolume(1.0f, NULL);
                std::wcout << L"Volume of " << g_sessionNames[i] << L" unmuted." << std::endl;
            }
            return;
        }
    }
}
