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
#include <fileapi.h>
#include <condition_variable>

using namespace std;

// Forward declaration for struct AudioDevice
struct MultimediaButton {
    QString name;
    std::vector<int> keyCodes; // Jetzt ein Vektor von Tastencodes

    // Konstruktor, der einen einzelnen KeyCode oder eine Liste von KeyCodes akzeptiert
    MultimediaButton(QString name, int keyCode) : name(name), keyCodes({keyCode}) {}
    MultimediaButton(QString name, const std::vector<int>& keyCodes) : name(name), keyCodes(keyCodes) {}

    MultimediaButton(): name(""), keyCodes({}) {} // Standardkonstruktor
};

struct AudioDevice {
    QString name;
    IAudioEndpointVolume* endpointVolume;
};

// Function prototypes
vector<AudioDevice> GetAudioSessionOutputs();
void mainLoop(const vector<AudioDevice>& audioDevices, vector<MultimediaButton>& keyMaps, const WCHAR* com, std::atomic<bool>& shouldStop, std::condition_variable& threadTerminated);
HANDLE ConnectToSerial(const WCHAR* com);
QStringList getAvailableComPortsQt();


