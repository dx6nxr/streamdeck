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

using namespace std;

// Forward declaration for struct AudioDevice
struct AudioDevice;

// Function prototypes
vector<AudioDevice> GetAudioSessionOutputs();
void ChangeVolumeForAllDevices(const vector<AudioDevice>& audioDevices);
void mainLoop(const vector<AudioDevice>& audioDevices, HANDLE hSerial);
HANDLE ConnectToSerial(const WCHAR* com);

// Struct to store information about an audio device
struct AudioDevice {
	wstring name;
	IAudioEndpointVolume* endpointVolume;
};

int main();