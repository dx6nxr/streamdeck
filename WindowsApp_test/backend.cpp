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


using namespace std;

struct AudioDevice {
    wstring name;
    IAudioEndpointVolume* endpointVolume;
};

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

void ChangeVolumeForAllDevices(const vector<AudioDevice>& audioDevices) {
    for (const auto& audioDevice : audioDevices) {
        if (audioDevice.endpointVolume) {
            float volumeLevel = 0.0f;
            HRESULT hr = audioDevice.endpointVolume->GetMasterVolumeLevelScalar(&volumeLevel);
            if (SUCCEEDED(hr)) {
                wcout << "Current Volume Level for " << wstring(audioDevice.name) << ": " << volumeLevel * 100 << "%" << endl;

                // Prompt user for new volume level
                float newVolume;
                wcout << "Enter new volume level for " << wstring(audioDevice.name) << " (0.0 to 1.0): ";
                cin >> newVolume;

                // Clamp the volume level between 0.0 and 1.0
                if (newVolume < 0.0f) newVolume = 0.0f;
                if (newVolume > 1.0f) newVolume = 1.0f;

                // Set the new volume level
                hr = audioDevice.endpointVolume->SetMasterVolumeLevelScalar(newVolume, nullptr);
                if (SUCCEEDED(hr)) {
                    cout << "Volume changed to: " << newVolume * 100 << "%" << endl;
                }

                else {
                    cout << "Failed to set the volume level." << endl;
                }
            }
            else {
                cout << "Failed to get the current volume level." << endl;
            }
        }
    }
}

void mainLoop(const vector<AudioDevice>& audioDevices, HANDLE hSerial) {
    //HRESULT hr = audioDevice.endpointVolume->GetMasterVolumeLevelScalar(&volumeLevel);
    // Read data from the serial port
    char buffer[256];
    DWORD bytesRead;
    float prevInputs[14]{};
    while (true) {
        if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {

            // buffer contains the volume level as a csv string of multiple values
            // split them by comma into an array of size 12
            char* token = strtok(buffer, ",");
            float inputs[14]{};
            int i = 0;
            while (i < 14) {
                inputs[i] = atof(token);
                token = strtok(NULL, ",");
                i++;
            }
            for (int i = 0; i < 14; i++) {
                cout << inputs[i] << " ";
            }
            cout << endl;


            //buffer[bytesRead] = '\0'; // Null-terminate the string
            //std::cout << "Read from COM5: " << buffer << std::endl;
            // get the volume level from the serial port that should be between 0 and 1 float
            //volumeLevel = atof(buffer) / 1023;
            //cout << "volume level: " << volumeLevel << endl;

            //checking the linear potentiometer values
            if (inputs[0] == -1 && inputs[13] == -1) {
                for (int i = 1; i < 5; i++) {
                    inputs[i] = inputs[i] / 1023;
                    // Clamp the volume level between 0.0 and 1.0
                    if (inputs[i] < 0.0f) inputs[i] = 0.0f;
                    if (inputs[i] > 1.0f) inputs[i] = 1.0f;

                    //changing the respected volume levels
                    if (abs(inputs[i] - prevInputs[i]) > 0.001) {
                        // Set the new volume level
                        HRESULT hr = audioDevices[i - 1].endpointVolume->SetMasterVolumeLevelScalar(inputs[i], nullptr);
                    }
                }
            }
            //cout << endl;
            //if (SUCCEEDED(hr)) {
            //	cout << "Volume changed to: " << volumeLevel * 100 << "%" << endl;
            //}
            //else {
            //	cout << "Failed to set the volume level." << endl;
            //}
            // sleep 100 ms
            for (int i = 0; i < 14; i++) {
                prevInputs[i] = inputs[i];
            }
            Sleep(100);
        }
        else {
            std::cerr << "Error reading from COM5" << std::endl;
        }
    }
    //cout << "setting the volume level to " << volumeLevel << endl;
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
    dcbSerialParams.BaudRate = CBR_115200; // Set baud rate
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

int main() {

    // Create a vector to store the audio devices
    vector<AudioDevice> audioDevices;

    // List audio session outputs and store them in the vector
    audioDevices = GetAudioSessionOutputs();

    // Open the serial port
    const WCHAR* com = L"\\\\.\\COM5";
    HANDLE hSerial = ConnectToSerial(com);

    if (hSerial == nullptr) {
        cerr << "Error connecting to serial port" << endl;

    }

    // create an array of the devices that should be managed by the serial port volume level
    // prompt user to choose 4 devices and save them in an array
    // then use the array to change the volume level of the devices
    vector<AudioDevice> chosenDevices;
    cout << "Choose 4 devices to manage by the serial port volume level" << endl;
    for (int i = 0; i < 4; i++) {
        cout << "Input device number:" << endl;
        int num;
        cin >> num;
        if (num >= 0 && num < audioDevices.size()) {
            chosenDevices.push_back(audioDevices[num]);
        }
    }

    mainLoop(chosenDevices, hSerial);

    // Release the endpoint volume interfaces
    for (auto& audioDevice : audioDevices) {
        if (audioDevice.endpointVolume) {
            audioDevice.endpointVolume->Release();
        }
    }

    return 0;
}

