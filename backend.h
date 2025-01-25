#pragma once

#include <atomic>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <Windows.h>
#include <comdef.h>
#include <propkey.h>
#include <propvarutil.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <qcontainerfwd.h>
#include <qobject.h>
#include <vector>
#include <string>
#include <fileapi.h>
#include <condition_variable>

using namespace std;

// Forward declaration for struct AudioDevice
struct AudioDevice {
    wstring name;
    IAudioEndpointVolume* endpointVolume;
};

struct MultimediaButton {
    QString name;
    unsigned short int keyCode;

    MultimediaButton(QString name, unsigned short int keyCode) {
        this->name = name;
        this->keyCode = keyCode;
    }
};

// Function prototypes
vector<AudioDevice> GetAudioSessionOutputs();
QString getName(unsigned short int keyCode, vector<MultimediaButton> keys);
void mainLoop(const vector<AudioDevice>& audioDevices, vector<unsigned short int>& keyMaps, const WCHAR* com, std::atomic<bool>& shouldStop, std::condition_variable& threadTerminated);
HANDLE ConnectToSerial(const WCHAR* com);
QStringList getAvailableComPortsQt();


