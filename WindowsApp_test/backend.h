#pragma once

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
#include <condition_variable>

using namespace std;

// Forward declaration for struct AudioDevice
struct AudioDevice;

// Function prototypes
vector<AudioDevice> GetAudioSessionOutputs();
void mainLoop(const vector<AudioDevice>& audioDevices, vector<unsigned short int>& keyMaps, const WCHAR* com, std::atomic<bool>& shouldStop, std::condition_variable& threadTerminated);
HANDLE ConnectToSerial(const WCHAR* com);
std::vector <wstring> getAvailableComPorts();

// Struct to store information about an audio device
struct AudioDevice {
	wstring name;
	IAudioEndpointVolume* endpointVolume;
};