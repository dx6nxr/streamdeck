#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <Windows.h>
#include <comdef.h>
#include <propkey.h>
#include <propvarutil.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <vector>
#include <string>
#include <fileapi.h>
#include <winuser.h>
// for the serial port
#include <setupapi.h>
#include <devguid.h>
#include <list>
#include <condition_variable>
#include <fstream>

#pragma comment(lib, "Setupapi.lib")

#define SLIDERS_COUNT 4
#define KEYS_COUNT 8
#define PADDING_COUNT 2
#define INPUT_LEN (SLIDERS_COUNT + KEYS_COUNT + PADDING_COUNT)

#define SIGNAL_THRESHOLD 2

using namespace std;

float prevInputs[SLIDERS_COUNT + KEYS_COUNT + PADDING_COUNT]{};
float inputs[SLIDERS_COUNT + KEYS_COUNT + PADDING_COUNT]{};
const vector<unsigned short int> key_maps = { 0xB3, 0xB1, 0xB0, 0xB5, 0x7C, 0x7D, 0x7E, 0x7F};

std::ofstream outfile("output.txt");
std::streambuf* old_cout_buf = std::cout.rdbuf();
std::streambuf* old_cerr_buf = std::cerr.rdbuf();

struct AudioDevice {
    wstring name;
    IAudioEndpointVolume* endpointVolume;
};

void SimulateKeyPress(int vKey) {
    keybd_event(vKey, 0xbf, 0, 0); // Press the key
    keybd_event(vKey, 0xbf, KEYEVENTF_KEYUP, 0); // Release the key
}

vector<AudioDevice> GetAudioSessionOutputs() {
    CoInitialize(nullptr);
    vector<AudioDevice> audioDevices;

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator
    );

    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* deviceCollection = nullptr;
        hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);

        if (SUCCEEDED(hr)) {
            UINT count;
            deviceCollection->GetCount(&count);
            // cout << "Number of audio output devices: " << count << endl;

            for (UINT i = 0; i < count; ++i) {
                IMMDevice* device = nullptr;
                hr = deviceCollection->Item(i, &device);
                if (SUCCEEDED(hr)) {
                    IPropertyStore* propertyStore = nullptr;
                    hr = device->OpenPropertyStore(STGM_READ, &propertyStore);

                    if (SUCCEEDED(hr)) {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &varName);

                        if (SUCCEEDED(hr)) {
                            // wcout << i << L") Device: " << varName.pwszVal << endl;

                            // Create an AudioDevice struct and store it
                            AudioDevice audioDevice;
                            audioDevice.name = varName.pwszVal;
                            audioDevice.endpointVolume = nullptr;

                            // Activate the IAudioEndpointVolume interface
                            hr = device->Activate(
                                __uuidof(IAudioEndpointVolume),
                                CLSCTX_INPROC_SERVER,
                                nullptr,
                                (void**)&audioDevice.endpointVolume
                            );

                            if (SUCCEEDED(hr)) {
                                audioDevices.push_back(audioDevice); // Add to the list
                            }
                            else {
                                cerr << "Failed to activate endpoint volume." << endl;
                            }

                            PropVariantClear(&varName);
                        }
                        else {
                            cerr << "Failed to retrieve the name" << endl;
                        }

                        propertyStore->Release();
                    }
                    else {
                        cout << "Failed to get properties" << endl;
                    }
                    device->Release();
                }
                else {
                    cerr << "Failed to get device" << endl;
                }
            }
            deviceCollection->Release();
        }
        deviceEnumerator->Release();
    }

    CoUninitialize();
	return audioDevices;
}

void applyInputs(const vector<AudioDevice>& audioDevices) {
    //the inputs 1 to 4 are the volume levels
    //the inputs 5 to 14 are the multimedia buttons
    for (int i = 1; i < INPUT_LEN-1; i++) {
        if (i <= SLIDERS_COUNT) {
            // if the value is different from the previous one by at least SIGNAL_THRESHOLD
            if (abs(inputs[i] - prevInputs[i]) > SIGNAL_THRESHOLD) {
                float volumeLevel = inputs[i] / 1023;
                if (volumeLevel < 0) volumeLevel = 0.0f;
                if (volumeLevel > 1) volumeLevel = 1.0f;
                audioDevices[i - 1].endpointVolume->SetMasterVolumeLevelScalar(volumeLevel, nullptr);
            }
        }
        else {
            // keypress is change from 1 to 0
            if (inputs[i] == 0 && prevInputs[i] == 1) {
                SimulateKeyPress(key_maps[i - SLIDERS_COUNT - 1]);
            }
        }
        prevInputs[i] = inputs[i];
    }
}

void mainLoop(const vector<AudioDevice>& audioDevices, HANDLE hSerial, std::atomic<bool>& shouldStop, std::condition_variable& threadTerminated) {
    DWORD bytesRead;
    char buffer[256];
    std::cout.rdbuf(outfile.rdbuf());
    std::cerr.rdbuf(outfile.rdbuf());

    std::string lineBuffer;

    while (!shouldStop) {
        memset(buffer, 0, sizeof(buffer));
        if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                lineBuffer += std::string(buffer, bytesRead);

                size_t startPos = lineBuffer.find("-1,");
                if (startPos != std::string::npos) {
                    size_t endPos = lineBuffer.find(",-1");
                    if (endPos != std::string::npos && endPos > startPos) {
                        std::string line = lineBuffer.substr(startPos, endPos - startPos + 3);
                        lineBuffer.erase(startPos, endPos - startPos + 3);

                        // Parse the input line
                        char* token = strtok((char*)line.c_str(), ",");
                        int i = 0;
                        while (i < INPUT_LEN && token != NULL) {
                            inputs[i] = atof(token);
                             cout << inputs[i] << " "; // Uncomment for debugging
                            token = strtok(NULL, ",");
                            i++;
                        }
                         cout << endl; // Uncomment for debugging

                        applyInputs(audioDevices);
                    }
                }
            }
        }
        else {
            std::cerr << "Error reading from COM5" << std::endl;
        }
    }

    threadTerminated.notify_all();
}


HANDLE ConnectToSerial(const WCHAR* com) {
    // Open the serial port
    HANDLE hSerial = CreateFile(com,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening COM5" << std::endl;
        return nullptr;
    }
    // Set the serial port parameters
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error getting state" << std::endl;
        CloseHandle(hSerial);
        return nullptr;
    }
    dcbSerialParams.BaudRate = CBR_57600; // Set baud rate
    dcbSerialParams.ByteSize = 8;         // Data size
    dcbSerialParams.StopBits = ONESTOPBIT; // Stop bits
    dcbSerialParams.Parity = NOPARITY;    // Parity
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error setting state" << std::endl;
        CloseHandle(hSerial);
        return nullptr;
    }
    // Set timeouts
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hSerial, &timeouts);

    return hSerial;
}

std::vector<wstring> getAvailableComPorts() 
{
    wchar_t lpTargetPath[5000]; // buffer to store the path of the COM PORTS
    list<int> portNumbers;

    for (int i = 0; i < 255; i++) // checking ports from COM0 to COM255
    {
        wstring str = L"COM" + to_wstring(i); // converting to COM0, COM1, COM2
        DWORD res = QueryDosDevice(str.c_str(), lpTargetPath, 5000);

        // Test the return value and error if any
        if (res != 0) //QueryDosDevice returns zero if it didn't find an object
        {
            portNumbers.push_back(i);
            //std::cout << str << ": " << lpTargetPath << std::endl;
        }
        if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
        }
    }

    std::vector<wstring> portList;
    for (auto i : portNumbers) {
        portList.push_back(L"COM" + to_wstring(i));
    }
    return portList;
}