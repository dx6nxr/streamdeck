#include <atomic>
#include <qmessagebox.h>
#include <thread>
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <Windows.h>
#include <comdef.h>
#include <propkey.h>
#include <propvarutil.h>
#include <initguid.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <vector>
#include <string>
#include <fileapi.h>
#include <winuser.h>
// for the serial port
#include <setupapi.h>
#include <devguid.h>
#include <QStringList>
#include <condition_variable>
#include <fstream>
// other parts of the app
#include "backend.h"

#pragma comment(lib, "Setupapi.lib")

#define SLIDERS_COUNT 4
#define KEYS_COUNT 8
#define PADDING_COUNT 2
#define INPUT_LEN (SLIDERS_COUNT + KEYS_COUNT + PADDING_COUNT)

#define SIGNAL_THRESHOLD 1

using namespace std;

float prevInputs[SLIDERS_COUNT + KEYS_COUNT + PADDING_COUNT]{};
float inputs[SLIDERS_COUNT + KEYS_COUNT + PADDING_COUNT]{};
const vector<unsigned short int> key_maps = { 0xB3, 0xB1, 0xB0, 0xB5, 0x7C, 0x7D, 0x7E, 0x7F};

std::ofstream outfile("output.txt");
std::streambuf* old_cout_buf = std::cout.rdbuf();
std::streambuf* old_cerr_buf = std::cerr.rdbuf();

void SimulateKeyPress(MultimediaButton vKey) {
    for (int vKey : vKey.keyCodes) {
        keybd_event(vKey, 0xbf, 0, 0); // Drücke die Taste
    }

    for (int vKey : vKey.keyCodes) {
        keybd_event(vKey, 0xbf, KEYEVENTF_KEYUP, 0); // Lasse die Taste los
    }
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
                            wchar_t *name = varName.pwszVal;
                            audioDevice.name = QString::fromWCharArray(name);
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

void applyInputs(const vector<AudioDevice>& audioDevices, vector<MultimediaButton>& keyMaps) {
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
                SimulateKeyPress(keyMaps[i - SLIDERS_COUNT - 1]);
            }
        }
        prevInputs[i] = inputs[i];
    }
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
        std::wcerr << L"Error opening " << com <<std::endl;
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

void mainLoop(const vector<AudioDevice>& audioDevices, vector<MultimediaButton>& keyMaps, const WCHAR* com, std::atomic<bool>& shouldStop, std::condition_variable& threadTerminated) {
    DWORD bytesRead;
    char buffer[256];
    std::cout.rdbuf(outfile.rdbuf());
    std::cerr.rdbuf(outfile.rdbuf());
    HANDLE hSerial = ConnectToSerial(com);
    bool connected = hSerial != nullptr;

    std::string lineBuffer;

    while (!shouldStop) {
        if (connected && ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                // if lineBuffer is too long, clear it
                if (lineBuffer.length() > 256) {
                    lineBuffer.clear();
                }
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
                            //cout << inputs[i] << " "; // Uncomment for debugging
                            token = strtok(NULL, ",");
                            i++;
                        }
                        //cout << endl; // Uncomment for debugging

                        applyInputs(audioDevices, keyMaps);
                    }
                }
            }
        }
        else {
            //std::cout << "Trying to reconnect to serial." << endl;
            connected = false;
            while (!shouldStop && !connected) {
                // disconnect from serial if it is connected
                if (hSerial != nullptr) {
                    CloseHandle(hSerial);
                    hSerial = nullptr;
                }
                hSerial = ConnectToSerial(com);
                connected = hSerial != nullptr;
                if (connected) {
                    std::cout << "Reconnected to serial." << endl;
                }
                else {
                    // sleep for 5 seconds
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
        }
        threadTerminated.notify_all();
    }
}

QStringList getAvailableComPortsQt() {
    QStringList portList;
    for (int i = 0; i < 255; i++) {
        QString portName = QString("COM%1").arg(i);
        wchar_t lpTargetPath[5000];
        DWORD res = QueryDosDevice(reinterpret_cast<const wchar_t*>(portName.utf16()), lpTargetPath, 5000);
        if (res != 0) {
            portList << portName;
        }
        if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            // Handle the error (e.g., log a message)
        }
    }
    return portList;
}
