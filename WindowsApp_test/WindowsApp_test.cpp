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
HWND slider1ChoiceBox;
HWND slider2ChoiceBox;
HWND slider3ChoiceBox;
HWND slider4ChoiceBox;
HWND comboBoxChooseComHandle;

HWND comboBoxBtnPreset1;
HWND comboBoxBtnPreset2;
HWND comboBoxBtnPreset3;
HWND comboBoxBtnPreset4;
HWND comboBoxBtnPreset5;
HWND comboBoxBtnPreset6;
HWND comboBoxBtnPreset7;
HWND comboBoxBtnPreset8;

HWND updateDevicesButton;
HWND savePresetButtonHandle;
HWND connectSerialButtonHandle;
HWND loadPresetButtonHandle;


// backend global vars
vector<AudioDevice> audioDevices = GetAudioSessionOutputs();
wstring com;
HANDLE hSerial;
vector<AudioDevice> chosenDevices(4);
vector<unsigned short int> keyMaps(8);
std::vector <wstring> comPorts;
std::thread workerThread;
std::atomic<bool> shouldStop(false);
std::condition_variable threadTerminated;
std::mutex mtx;
std::unique_lock<std::mutex> threadTerminationLock(mtx);

struct MultimediaButton {
    std::wstring name;
    unsigned short int keyCode;

    MultimediaButton(const std::wstring& name, unsigned short int keyCode)
        : name(name), keyCode(keyCode) {
    }

    static MultimediaButton get(unsigned short int keyCode, vector <MultimediaButton> buttonMaps) {
        for (int i = 0; i < buttonMaps.size(); i++) {
            if (buttonMaps[i].keyCode == keyCode) {
                return buttonMaps[i];
            }
        }
    }
};

std::vector<MultimediaButton> buttonMaps = {
            MultimediaButton(L"F13", 0x7C),
            MultimediaButton(L"F14", 0x7D),
            MultimediaButton(L"F15", 0x7E),
            MultimediaButton(L"F16", 0x7F),
            MultimediaButton(L"F17", 0x80),
            MultimediaButton(L"F18", 0x81),
            MultimediaButton(L"F19", 0x82),
            MultimediaButton(L"F20", 0x83),
            MultimediaButton(L"Play/pause", 0xB3),
            MultimediaButton(L"Next track", 0xB0),
            MultimediaButton(L"Previous track", 0xB1),
            MultimediaButton(L"Stop media", 0xB2),
            MultimediaButton(L"Select media", 0xB5),
            MultimediaButton(L"NUM LOCK", 0x90),
            MultimediaButton(L"SCROLL LOCK", 0x91),
            MultimediaButton(L"Browser back", 0xA6),
            MultimediaButton(L"Browser forward", 0xA7)
};

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
	SendMessage(slider1ChoiceBox, CB_RESETCONTENT, 0, 0);
	SendMessage(slider2ChoiceBox, CB_RESETCONTENT, 0, 0);
	SendMessage(slider3ChoiceBox, CB_RESETCONTENT, 0, 0);
	SendMessage(slider4ChoiceBox, CB_RESETCONTENT, 0, 0);

	for (int i = 0; i < audioDevices.size(); i++) {
		SendMessage(slider1ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
		SendMessage(slider2ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
		SendMessage(slider3ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
		SendMessage(slider4ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	}
}

static void UpdateButtonMapsLists(HWND hWnd) {
    for (int i = 0; i < buttonMaps.size(); i++) {
        SendMessage(comboBoxBtnPreset1, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
        SendMessage(comboBoxBtnPreset2, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
        SendMessage(comboBoxBtnPreset3, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
        SendMessage(comboBoxBtnPreset4, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
        SendMessage(comboBoxBtnPreset5, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
        SendMessage(comboBoxBtnPreset6, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
        SendMessage(comboBoxBtnPreset7, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
        SendMessage(comboBoxBtnPreset8, CB_ADDSTRING, 0, (LPARAM)buttonMaps[i].name.c_str());
    }
}

void loadPreset(HWND hWnd) {
    Configuration config = readFromJson();
    if (config.isValid) {
        chosenDevices.clear();
        for (int i = 0; i < config.sliders.size(); i++) {
            for (int j = 0; j < audioDevices.size(); j++) {
                if (config.sliders[i] == wstring_to_string(audioDevices[j].name)) {
                    chosenDevices.push_back(audioDevices[j]);
                }
            }
        }

        SendMessage(comboBoxChooseComHandle, CB_SELECTSTRING, -1, (LPARAM)config.com_port.c_str());
        SendMessage(slider1ChoiceBox, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[0].name.c_str());
        SendMessage(slider2ChoiceBox, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[1].name.c_str());
        SendMessage(slider3ChoiceBox, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[2].name.c_str());
        SendMessage(slider4ChoiceBox, CB_SELECTSTRING, -1, (LPARAM)chosenDevices[3].name.c_str());

        com = config.com_port;

        keyMaps.clear();
        for (int i = 0; i < config.buttons.size(); i++) {
            keyMaps.push_back(config.buttons[i]);
        }
		SendMessageW(comboBoxBtnPreset7, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[0], buttonMaps).name.c_str());
		SendMessageW(comboBoxBtnPreset5, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[1], buttonMaps).name.c_str());
		SendMessageW(comboBoxBtnPreset3, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[2], buttonMaps).name.c_str());
		SendMessageW(comboBoxBtnPreset1, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[3], buttonMaps).name.c_str());
		SendMessageW(comboBoxBtnPreset8, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[4], buttonMaps).name.c_str());
		SendMessageW(comboBoxBtnPreset6, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[5], buttonMaps).name.c_str());
		SendMessageW(comboBoxBtnPreset4, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[6], buttonMaps).name.c_str());
		SendMessageW(comboBoxBtnPreset2, CB_SELECTSTRING, -1, (LPARAM)MultimediaButton::get(keyMaps[7], buttonMaps).name.c_str());
    }
    else {
		MessageBox(hWnd, L"Invalid JSON file", L"Error", MB_OK);
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
   updateDevicesButton = CreateWindowEx(
       0, L"BUTTON", L"Update audio devices", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
       10, 40, 150, 50, hWnd, (HMENU)IDB_BUTTON, hInstance, NULL);

   savePresetButtonHandle = CreateWindowEx(
       0, L"BUTTON", L"Save preset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
       170, 40, 150, 50, hWnd, (HMENU)IDB_SAVE_PRESET_BTN, hInstance, NULL);

   loadPresetButtonHandle = CreateWindowEx(
       0, L"BUTTON", L"Load preset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
       330, 40, 150, 50, hWnd, (HMENU)IDB_LOAD_PRESET_BTN, hInstance, NULL);

   connectSerialButtonHandle = CreateWindowEx(
	   0, L"BUTTON", L"Connect to serial", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
	   490, 40, 150, 50, hWnd, (HMENU)IDB_CONNECT_SERIAL_BTN, hInstance, NULL);

   if (!updateDevicesButton || !savePresetButtonHandle || !connectSerialButtonHandle) {
       return FALSE;
   }

   // slider binding boxes
   slider1ChoiceBox = CreateWindowEx(
       0, WC_COMBOBOX, L"slider1", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       10, 10, 250, 200, hWnd, (HMENU)IDC_COMBOBOX1, hInstance, NULL);

   slider2ChoiceBox = CreateWindowEx(
       0, WC_COMBOBOX, L"slider2", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       270, 10, 250, 200, hWnd, (HMENU)IDC_COMBOBOX2, hInstance, NULL);

   slider3ChoiceBox = CreateWindowEx(
       0, WC_COMBOBOX, L"slider3", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       530, 10, 250, 200, hWnd, (HMENU)IDC_COMBOBOX3, hInstance, NULL);

   slider4ChoiceBox = CreateWindowEx(
       0, WC_COMBOBOX, L"slider4", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       790, 10, 250, 200, hWnd, (HMENU)IDC_COMBOBOX4, hInstance, NULL);

   // com port choice box
   comboBoxChooseComHandle = CreateWindowEx(
	   0, WC_COMBOBOX, L"com port", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
	   1050, 10, 70, 200, hWnd, (HMENU)IDC_CHOOSE_SERIAL, hInstance, NULL);

   // button binding boxes
   comboBoxBtnPreset1 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button4", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1050, 50, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX1, hInstance, NULL);

   comboBoxBtnPreset2 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button8", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1160, 50, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX2, hInstance, NULL);

   comboBoxBtnPreset3 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button3", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1050, 100, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX3, hInstance, NULL);

   comboBoxBtnPreset4 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button7", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1160, 100, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX4, hInstance, NULL);

   comboBoxBtnPreset5 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button2", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1050, 150, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX5, hInstance, NULL);

   comboBoxBtnPreset6 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button6", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1160, 150, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX6, hInstance, NULL);

   comboBoxBtnPreset7 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button1", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1050, 200, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX7, hInstance, NULL);

   comboBoxBtnPreset8 = CreateWindowEx(
       0, WC_COMBOBOX, L"Button5", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
       1160, 200, 100, 200, hWnd, (HMENU)IDC_BUTTONBOX8, hInstance, NULL);

   if (!slider1ChoiceBox or !slider2ChoiceBox or !slider3ChoiceBox or !slider4ChoiceBox) {
       return FALSE;
   }

   for (int i = 0; i < audioDevices.size(); i++) {
	   SendMessage(slider1ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	   SendMessage(slider2ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	   SendMessage(slider3ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
	   SendMessage(slider4ChoiceBox, CB_ADDSTRING, 0, (LPARAM)audioDevices[i].name.c_str());
   }

   UpdateButtonMapsLists(hWnd);

   comPorts = getAvailableComPorts();
   SendDlgItemMessage(hWnd, IDC_CHOOSE_SERIAL, CB_ADDSTRING, 0, (LPARAM)L"Choose COM port");
   for (int i = 0; i < comPorts.size(); i++) {
	   SendDlgItemMessage(hWnd, IDC_CHOOSE_SERIAL, CB_ADDSTRING, 0, (LPARAM)comPorts[i].c_str());
   }

   loadPreset(hWnd);

   //SendMessage(slider1ChoiceBox, CB_ADDSTRING, 0, (LPARAM)L"Item 1");
   //SendMessage(slider1ChoiceBox, CB_ADDSTRING, 0, (LPARAM)L"Item 2");
   //SendMessage(slider1ChoiceBox, CB_ADDSTRING, 0, (LPARAM)L"Item 3");

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
                audioDevices = GetAudioSessionOutputs();
                UpdateDropDowns(hWnd);
            }
            break;
        case IDB_SAVE_PRESET_BTN:
            if (wmEvent == BN_CLICKED) {
                std::vector<std::string> devices;
                for (int i = 0; i < chosenDevices.size(); i++) {
                    devices.push_back(wstring_to_string(chosenDevices[i].name));
                        //mainLoop(chosenDevices, hSerial);
                }
                writeToJson(devices, com, keyMaps);
            }
            break;
        case IDB_LOAD_PRESET_BTN:
            if (wmEvent == BN_CLICKED) {
				loadPreset(hWnd);
                //mainLoop(chosenDevices, hSerial);
            }
            break;
        case IDB_CONNECT_SERIAL_BTN:
            if (wmEvent == BN_CLICKED) {
                    workerThread = std::thread(mainLoop, std::ref(chosenDevices), std::ref(keyMaps), com.c_str(), std::ref(shouldStop), std::ref(threadTerminated));
            }
            break;
        case IDC_COMBOBOX1:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(slider1ChoiceBox, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(slider1ChoiceBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    chosenDevices[0] = (audioDevices[selectedIndex]);
                    //MessageBox(hWnd, selectedText, audioDevices[selectedIndex].name.c_str(), MB_OK);
                }
            }
            break;
        case IDC_COMBOBOX2:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(slider2ChoiceBox, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(slider2ChoiceBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    chosenDevices[1] = (audioDevices[selectedIndex]);
                    //MessageBox(hWnd, selectedText, audioDevices[selectedIndex].name.c_str(), MB_OK);
                }
            }
            break;
        case IDC_COMBOBOX3:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(slider3ChoiceBox, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(slider3ChoiceBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
                    chosenDevices[2] = (audioDevices[selectedIndex]);
                    //MessageBox(hWnd, selectedText, audioDevices[selectedIndex].name.c_str(), MB_OK);
                }
            }
            break;
        case IDC_COMBOBOX4:
            if (wmEvent == CBN_SELCHANGE) {
                int selectedIndex = SendMessage(slider4ChoiceBox, CB_GETCURSEL, 0, 0);
                if (selectedIndex != CB_ERR) {
                    wchar_t selectedText[256];
                    SendMessage(slider4ChoiceBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedText);
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
        case IDC_BUTTONBOX1:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset1, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[3] = buttonMaps[selectedIndex].keyCode;
				}
			}
            break;
		case IDC_BUTTONBOX2:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset2, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[7] = buttonMaps[selectedIndex].keyCode;
				}
			}
			break;
		case IDC_BUTTONBOX3:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset3, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[2] = buttonMaps[selectedIndex].keyCode;
				}
			}
			break;
		case IDC_BUTTONBOX4:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset4, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[6] = buttonMaps[selectedIndex].keyCode;
				}
			}
			break;
		case IDC_BUTTONBOX5:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset5, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[1] = buttonMaps[selectedIndex].keyCode;
				}
			}
			break;
		case IDC_BUTTONBOX6:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset6, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[5] = buttonMaps[selectedIndex].keyCode;
				}
			}
			break;
		case IDC_BUTTONBOX7:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset7, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[0] = buttonMaps[selectedIndex].keyCode;
				}
			}
			break;
		case IDC_BUTTONBOX8:
			if (wmEvent == CBN_SELCHANGE) {
				int selectedIndex = SendMessage(comboBoxBtnPreset8, CB_GETCURSEL, 0, 0);
				if (selectedIndex != CB_ERR) {
					keyMaps[4] = buttonMaps[selectedIndex].keyCode;
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
