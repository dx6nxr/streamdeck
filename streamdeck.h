#ifndef STREAMDECK_H
#define STREAMDECK_H

#include <QMainWindow>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <Windows.h>
#include <comdef.h>
#include <propkey.h>
#include <propvarutil.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <fileapi.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include "backend.h"
#include <qthread.h>

void showDisconnectedMessage();
void showConnectedMessage();

QT_BEGIN_NAMESPACE
namespace Ui {
class streamdeck;
}
QT_END_NAMESPACE

class streamdeck : public QMainWindow
{
    Q_OBJECT
    QThread *workerThread;

public:
    streamdeck(QWidget *parent = nullptr);
    ~streamdeck();

private slots: // Qt slots for handling events
    void updateAudioDevices();
    void savePreset();
    void loadPreset();
    void connectToSerial();
    void onComboBoxChange(int index);
    void onButtonBoxChange(int index);
    void updateButtonBoxes();

    void on_updateButton_clicked();

    void on_loadPresetBtn_clicked();

    void on_savePresetBtn_clicked();

    void on_connectSerialBtn_clicked();

    void on_addBinding_clicked();

    void on_comPortComboBox_currentTextChanged(const QString &arg1);

private:
    Ui::streamdeck *ui;

    // logic variables
    std::vector <AudioDevice> audioDevices; // All audio outputs
    std::vector <AudioDevice> chosenDevices; // Chosen devices

    QStringList comPorts; // Available com ports
    QString com; // The chosen com port
    HANDLE hSerial; //Serial handle for the chosen com port

    // Worker thread
    std::atomic<bool> shouldStop{false}; // Varible to signalize the worker thread to terminate
    std::condition_variable isThreadTerminated; // condvar indicating whether the thread terminated
    std::mutex mtx; // mutex for the condvar

    // UI elements
    std::vector <QComboBox*> comboBoxes;
    std::vector <QComboBox*> buttonBoxes;
    QComboBox *comPortComboBox;
    void createSliderBoxes();
    void createButtonBoxes();

};
#endif // STREAMDECK_H
