#include "streamdeck.h"
#include "backend.h"
#include "ui_streamdeck.h"
#include "presets.h"

//UI imports
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>
#include <qthread.h>

// config variables
int num_sliders = 4;
int num_buttons = 8;
int baud = 9600;

// logic variables
std::vector <AudioDevice> audioDevices; // All audio outputs
std::vector <AudioDevice> chosenDevices; // Chosen devices
std::vector<MultimediaButton> chosenKeyBinds; // maps for the buttons
std::vector<MultimediaButton> defaultButtonMaps = {
    MultimediaButton("F13", 0x7C),
    MultimediaButton("F14", 0x7D),
    MultimediaButton("F15", 0x7E),
    MultimediaButton("F16", 0x7F),
    MultimediaButton("F17", 0x80),
    MultimediaButton("F18", 0x81),
    MultimediaButton("F19", 0x82),
    MultimediaButton("F20", 0x83),
    MultimediaButton("Play/pause", 0xB3),
    MultimediaButton("Next track", 0xB0),
    MultimediaButton("Previous track", 0xB1),
    MultimediaButton("Stop media", 0xB2),
    MultimediaButton("Select media", 0xB5),
    MultimediaButton("NUM LOCK", 0x90),
    MultimediaButton("SCROLL LOCK", 0x91),
    MultimediaButton("Browser back", 0xA6),
    MultimediaButton("Copy", {VK_CONTROL, 'C'}),
    MultimediaButton("Paste", {VK_CONTROL, 'V'})
};

std::vector<MultimediaButton> buttonMaps = loadKeyBinds();

QStringList comPorts; // Available com ports
QString com; // The chosen com port
HANDLE hSerial; //Serial handle for the chosen com port

// Worker thread
QThread *wt = new QThread;
std::atomic<bool> shouldStop{false}; // Varible to signalize the worker thread to terminate
std::condition_variable isThreadTerminated; // condvar indicating whether the thread terminated
std::mutex mtx; // mutex for the condvar

// UI elements
std::vector <QComboBox*> comboBoxes;
std::vector <QComboBox> buttonBoxes;

streamdeck::streamdeck(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::streamdeck)
{
    ui->setupUi(this); // Initialize the UI first

    // Set the number of sliders
    chosenDevices.resize(num_sliders);
    chosenKeyBinds.resize(num_buttons);

    if (buttonMaps.empty()) {
        buttonMaps = defaultButtonMaps;
    }

    // Create UI elements
    comPortComboBox = findChild<QComboBox*>("comPortComboBox");
    comPortComboBox->addItem("Choose COM port");
    comPorts = getAvailableComPortsQt();
    comPortComboBox->addItems(comPorts);

    createSliderBoxes(); // Create the combo boxes for sliders
    updateAudioDevices();

    // Set up layout
    QGridLayout *gridLayout = findChild<QGridLayout*>("slidersGridLayout");

    int i = 0;
    for (auto comboBox : comboBoxes) {
        gridLayout->addWidget(comboBox, i, 0);
        comboBox->show();
        ++i;
    }
    createButtonBoxes(); // Create the combo boxes for buttons
    updateButtonBoxes();

    QGridLayout *buttonGridLayout = findChild<QGridLayout*>("buttonsGridLayout");
    i = 0;
    int j = 0;
    for (auto buttonBox : buttonBoxes) {
        if (i == 2) {
            i = 0;
            ++j;
        }
        buttonGridLayout->addWidget(buttonBox, j, i);
        buttonBox->show();
        ++i;
    }
    loadPreset();
}

void showDisconnectedMessage() {
    QMessageBox::warning(nullptr, "Error", "Could not connect to serial port.");
}

void showConnectedMessage() {
    QMessageBox::information(nullptr, "Success", "Connected to serial port.");
}

void streamdeck::savePreset(){
    if(writeToJson(chosenDevices, com.toStdWString(), chosenKeyBinds)){
        QMessageBox::information(this, "Success", "Preset saved successfully.");
    }
    else {
        QMessageBox::warning(this, "Error", "Could not save preset.");
    }
}

void streamdeck::loadPreset(){
    Configuration config = readFromJson();
    if (config.isValid) {
        //num_sliders = config.num_sliders;
        //num_buttons = config.num_btns;
        com = QString::fromStdWString(config.com_port);
        comPortComboBox->setCurrentText(com);
        // Set the sliders
        for (int i = 0; i < num_sliders; ++i) {
            comboBoxes[i]->setCurrentText(QString::fromStdString(config.sliders[i]));
        }
        // Set the buttons
        for (int i = 0; i < num_buttons; ++i) {
            buttonBoxes[i]->setCurrentText(config.bindings[i].name);
        }
    }
    else {
        QMessageBox::warning(this, "Error", "Invalid JSON structure in config.txt");
    }
}

void streamdeck::connectToSerial(){
    QObject::connect(wt, &QThread::started, [this] {
        mainLoop(chosenDevices, chosenKeyBinds, com.toStdWString().c_str(), shouldStop, isThreadTerminated);
    });
    wt->start();
}

void streamdeck::createSliderBoxes() {
    for (int i = 0; i < num_sliders; ++i) {
        QComboBox *comboBox = new QComboBox(this);
        comboBoxes.push_back(comboBox);
        connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &streamdeck::onComboBoxChange);
    }
}

void streamdeck::createButtonBoxes() {
    for (int i = 0; i < num_buttons; ++i) {
        QComboBox *buttonBox = new QComboBox(this);
        buttonBoxes.push_back(buttonBox);
        connect(buttonBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &streamdeck::onButtonBoxChange);
    }
}

void streamdeck::updateAudioDevices() {
    audioDevices = GetAudioSessionOutputs();

    for (auto comboBox : comboBoxes) {
        comboBox->clear();
        for (const auto& device : audioDevices) {
            comboBox->addItem(device.name);
        }
    }
}

void streamdeck::updateButtonBoxes() {
    for (int i = 0; i < num_buttons; ++i) {
        // remember the choice
        QString currentChoice = buttonBoxes[i]->currentText();

        buttonBoxes[i]->clear();
        for (const auto& button : buttonMaps) {
            buttonBoxes[i]->addItem(button.name);
        }
        // restore the choice
        buttonBoxes[i]->setCurrentText(currentChoice);
    }
}

void streamdeck::onComboBoxChange(int index) {
    // find the index of the comboBox that emitted the signal
    int comboBoxIndex = 0;
    for (int i = 0; i < comboBoxes.size(); ++i) {
        if (comboBoxes[i] == sender()) {
            comboBoxIndex = i;
            break;
        }
    }
    if (index >= 0 && index < audioDevices.size()) {
        chosenDevices[comboBoxIndex] = audioDevices[index];
    }
}

void streamdeck::onButtonBoxChange(int index) {
    int buttonBoxIndex = 0;
    for (int i = 0; i < buttonBoxes.size(); ++i) {
        if (buttonBoxes[i] == sender()) {
            buttonBoxIndex = i;
            break;
        }
    }
    if (index >= 0 && index < buttonMaps.size()) {
        chosenKeyBinds[buttonBoxIndex] = buttonMaps[index];
    }
}

streamdeck::~streamdeck()
{
    delete ui;
}

void streamdeck::on_updateButton_clicked()
{
    streamdeck::updateAudioDevices();
}


void streamdeck::on_loadPresetBtn_clicked()
{
    streamdeck::loadPreset();
}


void streamdeck::on_savePresetBtn_clicked()
{
    streamdeck::savePreset();
}


void streamdeck::on_connectSerialBtn_clicked()
{
    //get the index of the chosen com port
    int index = comPortComboBox->currentIndex()-1;
    if (index >= 0 && index < comPorts.size()) {
        com = comPorts[index];
    }
    streamdeck::connectToSerial();
}


void streamdeck::on_comPortComboBox_editTextChanged(const QString &arg1)
{
    // change the com port
    com = arg1;
}
vector<unsigned short int> getVirtualKeyCodeForModifiers(Qt::KeyboardModifiers key) {
    switch (key)
    {
        case Qt::ShiftModifier:
            return {VK_SHIFT};
        case Qt::ControlModifier:
            return {VK_CONTROL};
        case Qt::AltModifier:
            return {VK_MENU};
        case Qt::MetaModifier:
            return {VK_LWIN};
        case Qt::ControlModifier | Qt::ShiftModifier:
            return {VK_CONTROL, VK_SHIFT};
        case Qt::ControlModifier | Qt::AltModifier:
            return {VK_CONTROL, VK_MENU};
        case Qt::ControlModifier | Qt::MetaModifier:
            return {VK_CONTROL, VK_LWIN};
        case Qt::ShiftModifier | Qt::AltModifier:
            return {VK_SHIFT, VK_MENU};
        case Qt::ShiftModifier | Qt::MetaModifier:
            return {VK_SHIFT, VK_LWIN};
        case Qt::AltModifier | Qt::MetaModifier:
            return {VK_MENU, VK_LWIN};
        case Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier:
            return {VK_CONTROL, VK_SHIFT, VK_MENU};
        case Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier:
            return {VK_CONTROL, VK_SHIFT, VK_LWIN};
        case Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier:
            return {VK_CONTROL, VK_MENU, VK_LWIN};
        case Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier:
            return {VK_SHIFT, VK_MENU, VK_LWIN};
        case Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier:
            return {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN};
        default:
            return {};
    }
}

unsigned short int getVirtualKeyCode(Qt::Key key) {
    switch (key)
    {
    case Qt::Key_Escape:
        return VK_ESCAPE;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return VK_TAB;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Insert:
        return VK_INSERT;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Pause:
        return VK_PAUSE;
    case Qt::Key_Print:
        return VK_PRINT;
    case Qt::Key_Clear:
        return VK_CLEAR;
    case Qt::Key_Home:
        return VK_HOME;
    case Qt::Key_End:
        return VK_END;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Down:
        return VK_DOWN;
    case Qt::Key_PageUp:
        return VK_PRIOR;
    case Qt::Key_PageDown:
        return VK_NEXT;
    case Qt::Key_F1:
        return VK_F1;
    case Qt::Key_F2:
        return VK_F2;
    case Qt::Key_F3:
        return VK_F3;
    case Qt::Key_F4:
        return VK_F4;
    case Qt::Key_F5:
        return VK_F5;
    case Qt::Key_F6:
        return VK_F6;
    case Qt::Key_F7:
        return VK_F7;
    case Qt::Key_F8:
        return VK_F8;
    case Qt::Key_F9:
        return VK_F9;
    case Qt::Key_F10:
        return VK_F10;
    case Qt::Key_F11:
        return VK_F11;
    case Qt::Key_F12:
        return VK_F12;
    case Qt::Key_F13:
        return VK_F13;
    case Qt::Key_F14:
        return VK_F14;
    case Qt::Key_F15:
        return VK_F15;
    case Qt::Key_F16:
        return VK_F16;
    case Qt::Key_F17:
        return VK_F17;
    case Qt::Key_F18:
        return VK_F18;
    case Qt::Key_F19:
        return VK_F19;
    case Qt::Key_F20:
        return VK_F20;
    case Qt::Key_F21:
        return VK_F21;
    case Qt::Key_F22:
        return VK_F22;
    case Qt::Key_F23:
        return VK_F23;
    case Qt::Key_F24:
        return VK_F24;
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Asterisk:
        return VK_MULTIPLY;
    case Qt::Key_Plus:
        return VK_ADD;
    case Qt::Key_Comma:
        return VK_SEPARATOR;
    case Qt::Key_Minus:
        return VK_SUBTRACT;
    case Qt::Key_Slash:
        return VK_DIVIDE;
    case Qt::Key_MediaNext:
        return VK_MEDIA_NEXT_TRACK;
    case Qt::Key_MediaPrevious:
        return VK_MEDIA_PREV_TRACK;
    case Qt::Key_MediaPlay:
        return VK_MEDIA_PLAY_PAUSE;
    case Qt::Key_MediaStop:
        return VK_MEDIA_STOP;
    case Qt::Key_VolumeDown:
        return VK_VOLUME_DOWN;
    case Qt::Key_VolumeUp:
        return VK_VOLUME_UP;
    case Qt::Key_VolumeMute:
        return VK_VOLUME_MUTE;

        // numbers
    case Qt::Key_0:
        return 0x30;
    case Qt::Key_1:
        return 0x31;
    case Qt::Key_2:
        return 0x32;
    case Qt::Key_3:
        return 0x33;
    case Qt::Key_4:
        return 0x34;
    case Qt::Key_5:
        return 0x35;
    case Qt::Key_6:
        return 0x36;
    case Qt::Key_7:
        return 0x37;
    case Qt::Key_8:
        return 0x38;
    case Qt::Key_9:
        return 0x39;

        // letters
    case Qt::Key_A:
        return 0x41;
    case Qt::Key_B:
        return 0x42;
    case Qt::Key_C:
        return 0x43;
    case Qt::Key_D:
        return 0x44;
    case Qt::Key_E:
        return 0x45;
    case Qt::Key_F:
        return 0x46;
    case Qt::Key_G:
        return 0x47;
    case Qt::Key_H:
        return 0x48;
    case Qt::Key_I:
        return 0x49;
    case Qt::Key_J:
        return 0x4A;
    case Qt::Key_K:
        return 0x4B;
    case Qt::Key_L:
        return 0x4C;
    case Qt::Key_M:
        return 0x4D;
    case Qt::Key_N:
        return 0x4E;
    case Qt::Key_O:
        return 0x4F;
    case Qt::Key_P:
        return 0x50;
    case Qt::Key_Q:
        return 0x51;
    case Qt::Key_R:
        return 0x52;
    case Qt::Key_S:
        return 0x53;
    case Qt::Key_T:
        return 0x54;
    case Qt::Key_U:
        return 0x55;
    case Qt::Key_V:
        return 0x56;
    case Qt::Key_W:
        return 0x57;
    case Qt::Key_X:
        return 0x58;
    case Qt::Key_Y:
        return 0x59;
    case Qt::Key_Z:
        return 0x5A;

    default:
        return 0;
    }
}

void streamdeck::on_addBinding_clicked()
{
    // NewButtonBindingSequence
    // NewButtonBindingName
    // create a new MultimediaButton and add it to the buttonMaps
    QString name = findChild<QLineEdit*>("NewButtonBindingName")->text();
    QKeySequence  sequence = findChild<QKeySequenceEdit*>("NewButtonBindingSequence")->keySequence();
    //add the new button to the buttonMaps
    QDebug deb = qDebug();
    std::vector<int> keyCodes;
    for (int i = 0; i < sequence.count(); ++i) {
        QKeyCombination key = sequence[i];
        vector<unsigned short int> modifierKeyCode = getVirtualKeyCodeForModifiers(key.keyboardModifiers());
        unsigned short int keyCode = getVirtualKeyCode(key.key());

        // combine the modifierKeyCode with the keyCode and add it to the keyCodes
        keyCodes.insert(keyCodes.end(), modifierKeyCode.begin(), modifierKeyCode.end());
        keyCodes.push_back(keyCode);
    }
    // print the keyCodes into the console
    MultimediaButton newButton(name, keyCodes);
    buttonMaps.push_back(newButton);
    updateButtonBoxes();
    saveKeyBinds(buttonMaps);
}


