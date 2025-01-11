// WindowsApp_test.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include <windowsx.h>
#include "WindowsApp_test.h"
#include "backend.h"
#include <CommCtrl.h>
#include "SaveLoadPreset.h"
#include <string>
#include <locale>
#include <codecvt>
#include <thread>
#include <condition_variable>
#include <mutex>

std::string wstring_to_string(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// *** INSERT GLOBAL VARIABLES HERE ***
HWND comboBoxHandle1;
HWND comboBoxHandle2;
HWND comboBoxHandle3;
HWND comboBoxHandle4;
HWND comboBoxChooseComHandle;

HWND buttonHandle;
HWND savePresetButtonHandle;
HWND connectSerialButtonHandle;
HWND loadPresetButtonHandle;


// backend global vars
vector<AudioDevice> audioDevices = GetAudioSessionOutputs();
wstring com;
HANDLE hSerial;
vector<AudioDevice> chosenDevices{ 4 };
std::vector <wstring> comPorts;
std::thread workerThread;
std::atomic<bool> shouldStop(false);
std::condition_variable threadTerminated;
std::mutex mtx;
std::unique_lock<std::mutex> threadTerminationLock(mtx);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSAPPTEST, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSAPPTEST));

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

    return (int) msg.wParam;
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

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSAPPTEST));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WINDOWSAPPTEST);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

static void UpdateDropDowns(HWND hWnd) {
	SendMessage(comboBoxHandle1, CB_RESETCONTENT, 0, 0);
	SendMessage(comboBoxHandle2, CB_RESETCONTENT, 0, 0);
	SendMessage(comboBoxHandle3, CB_RESETCONTENT, 0, 0);
	SendMessage(comboBoxHandle4, CB_RESETCONTENT, 0, 0);

	for (int i = 0; i < audioDevices.size(); i++) {
		SendMessage(comboBoxHandle1, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
		SendMessage(comboBoxHandle2, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
		SendMessage(comboBoxHandle3, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
		SendMessage(comboBoxHandle4, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	}
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   // *** INSERT BUTTON AND COMBO BOX CREATION HERE ***
   buttonHandle = CreateWindowEx(
       0, L"BUTTON", L"Update audio devices", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
       10, 100, 150, 50, hWnd, (HMENU)IDB_BUTTON, hInstance, NULL);

   savePresetButtonHandle = CreateWindowEx(
       0, L"BUTTON", L"Save preset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
       10, 500, 150, 50, hWnd, (HMENU)IDB_SAVE_PRESET_BTN, hInstance, NULL);

   loadPresetButtonHandle = CreateWindowEx(
       0, L"BUTTON", L"Load preset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
       170, 500, 150, 50, hWnd, (HMENU)IDB_LOAD_PRESET_BTN, hInstance, NULL);

   connectSerialButtonHandle = CreateWindowEx(
	   0, L"BUTTON", L"Connect to serial", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
	   330, 500, 150, 50, hWnd, (HMENU)IDB_CONNECT_SERIAL_BTN, hInstance, NULL);

   if (!buttonHandle || !savePresetButtonHandle || !connectSerialButtonHandle) {
       return FALSE;
   }

   comboBoxHandle1 = CreateWindowEx(
       0, WC_COMBOBOX, L"slider1", CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
       10, 160, 200, 200, hWnd, (HMENU)IDC_COMBOBOX1, hInstance, NULL);

   comboBoxHandle2 = CreateWindowEx(
       0, WC_COMBOBOX, L"slider2", CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
       260, 160, 200, 200, hWnd, (HMENU)IDC_COMBOBOX2, hInstance, NULL);

   comboBoxHandle3 = CreateWindowEx(
       0, WC_COMBOBOX, L"slider3", CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
       510, 160, 200, 200, hWnd, (HMENU)IDC_COMBOBOX3, hInstance, NULL);

   comboBoxHandle4 = CreateWindowEx(
       0, WC_COMBOBOX, L"slider4", CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
       760, 160, 200, 200, hWnd, (HMENU)IDC_COMBOBOX4, hInstance, NULL);

   comboBoxChooseComHandle = CreateWindowEx(
	   0, WC_COMBOBOX, L"com port", CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
	   10, 0, 200, 200, hWnd, (HMENU)IDC_CHOOSE_SERIAL, hInstance, NULL);

   if (!comboBoxHandle1 or !comboBoxHandle2 or !comboBoxHandle3 or !comboBoxHandle4) {
       return FALSE;
   }

   for (int i = 0; i < audioDevices.size(); i++) {
	   SendMessage(comboBoxHandle1, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	   SendMessage(comboBoxHandle2, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	   SendMessage(comboBoxHandle3, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	   SendMessage(comboBoxHandle4, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
   }

   comPorts = getAvailableComPorts();
   SendDlgItemMessage(hWnd, IDC_CHOOSE_SERIAL, CB_ADDSTRING, 0, (LPARAM)L"Choose COM port");
   for (int i = 0; i < comPorts.size(); i++) {
	   SendDlgItemMessage(hWnd, IDC_CHOOSE_SERIAL, CB_ADDSTRING, 0, (LPARAM)comPorts[i].c_str());
   }

   //SendMessage(comboBoxHandle1, CB_ADDSTRING, 0, (LPARAM)L"Item 1");
   //SendMessage(comboBoxHandle1, CB_ADDSTRING, 0, (LPARAM)L"Item 2");
   //SendMessage(comboBoxHandle1, CB_ADDSTRING, 0, (LPARAM)L"Item 3");

   // *** END OF INSERTION ***

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

// ...

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//  i.e. button presses etc
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
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam); // Get the notification code
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDB_BUTTON:
            if (wmEvent == BN_CLICKED) {
                MessageBox(hWnd, L"Devices Updated!", L"Info", MB_OK);
                audioDevices = GetAudioSessionOutputs();
                UpdateDropDowns(hWnd);
            }
            break;
        case IDB_SAVE_PRESET_BTN:
            if (wmEvent == BN_CLICKED) {
                MessageBox(hWnd, L"Preset Saved!", L"Info", MB_OK);
                std::vector<std::string> devices;
                for (int i = 0; i < chosenDevices.size(); i++) {
                    devices.push_back(wstring_to_string(chosenDevices[i].name));
                        //mainLoop(chosenDevices, hSerial);
                }
                writeToJson(devices, com);
            }
            break;
        case IDB_LOAD_PRESET_BTN:
            if (wmEvent == BN_CLICKED) {
                MessageBox(hWnd, L"Preset Loaded!", L"Info", MB_OK);
                Configuration config = readFromJson();
                chosenDevices.clear();
                for (int i = 0; i < config.sliders.size(); i++) {
                    for (int j = 0; j < audioDevices.size(); j++) {
                        if (config.sliders[i] == wstring_to_string(audioDevices[j].name)) {
                            chosenDevices.push_back(audioDevices[j]);
                        }
                    }
                }
				com = config.com_port;
                SendMessage(comboBoxChooseComHandle, CB_SELECTSTRING, -1, (LPARAM)config.com_port.c_str());
                SendMessage(comboBoxHandle1, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[0].name.c_str());
                SendMessage(comboBoxHandle2, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[1].name.c_str());
                SendMessage(comboBoxHandle3, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[2].name.c_str());
                SendMessage(comboBoxHandle4, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[3].name.c_str());
                //mainLoop(chosenDevices, hSerial);
            }
            break;
        case IDB_CONNECT_SERIAL_BTN:
            if (wmEvent == BN_CLICKED) {

                hSerial = ConnectToSerial(com.c_str());

                if (hSerial != nullptr) {
                    MessageBox(hWnd, L"connected", L"Info", MB_OK);
                    workerThread = std::thread(mainLoop, std::ref(chosenDevices), hSerial, std::ref(shouldStop), std::ref(threadTerminated));
                }
                else {
                    MessageBox(hWnd, com.c_str(), L"Error", MB_OK);
                }
            }
            break;
        case IDC_COMBOBOX1:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(comboBoxHandle1, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(comboBoxHandle1, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    chosenDevices[0] = (audioDevices[selectedIndex]);
                    //MessageBox(hWnd, selectedText, audioDevices[selectedIndex].name.c_str(), MB_OK);
                }
            }
            break;
        case IDC_COMBOBOX2:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(comboBoxHandle2, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(comboBoxHandle2, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    chosenDevices[1] = (audioDevices[selectedIndex]);
                    //MessageBox(hWnd, selectedText, audioDevices[selectedIndex].name.c_str(), MB_OK);
                }
            }
            break;
        case IDC_COMBOBOX3:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(comboBoxHandle3, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(comboBoxHandle3, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    chosenDevices[2] = (audioDevices[selectedIndex]);
                    //MessageBox(hWnd, selectedText, audioDevices[selectedIndex].name.c_str(), MB_OK);
                }
            }
            break;
        case IDC_COMBOBOX4:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(comboBoxHandle4, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(comboBoxHandle4, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    chosenDevices[3] = (audioDevices[selectedIndex]);
                    //MessageBox(hWnd, selectedText, audioDevices[selectedIndex].name.c_str(), MB_OK);
                }
            }
            break;
        case IDC_CHOOSE_SERIAL:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(comboBoxChooseComHandle, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(comboBoxChooseComHandle, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    com = comPorts[selectedIndex - 1];
                    MessageBox(hWnd, com.c_str(), L"COM port", MB_OK);
                }
            }
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
        case WM_DESTROY:
            shouldStop = true;
			threadTerminated.wait(threadTerminationLock);
            if (workerThread.joinable()) {
                workerThread.join();
            }
            CloseHandle(hSerial);
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
