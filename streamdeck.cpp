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
std::vector <unsigned short int> chosenMaps; // maps for the buttons
std::vector<MultimediaButton> buttonMaps = {
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
};

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
    chosenMaps.resize(num_buttons);

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

void streamdeck::savePreset(){
    std::vector<std::string> sliders;
    for (int i = 0; i < num_sliders; ++i) {
        sliders.push_back(comboBoxes[i]->currentText().toStdString());
    }
    std::vector<unsigned short int> keyMaps;
    for (int i = 0; i < num_buttons; ++i) {
        keyMaps.push_back(chosenMaps[i]);
    }
    int index = comPortComboBox->currentIndex()-1;
    if (index >= 0 && index < comPorts.size()) {
        com = comPorts[index];
    }
    writeToJson(sliders, com.toStdWString(), keyMaps);
}

void streamdeck::loadPreset(){
    Configuration config = readFromJson();
    if (config.isValid) {
        num_sliders = config.num_sliders;
        num_buttons = config.num_btns;
        com = QString::fromStdWString(config.com_port);
        comPortComboBox->setCurrentText(com);
        // Set the sliders
        for (int i = 0; i < num_sliders; ++i) {
            comboBoxes[i]->setCurrentText(QString::fromStdString(config.sliders[i]));
        }
        // Set the buttons
        for (int i = 0; i < num_buttons; ++i) {
            buttonBoxes[i]->setCurrentText(getName(config.buttons[i], buttonMaps));
        }
    }
    else {
        QMessageBox::warning(this, "Error", "Invalid JSON structure in config.txt");
    }
}

void streamdeck::connectToSerial(){
    QObject::connect(wt, &QThread::started, [this] {
        mainLoop(chosenDevices, chosenMaps, com.toStdWString().c_str(), shouldStop, isThreadTerminated);
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
            comboBox->addItem(QString::fromStdWString(device.name));
        }
    }
}

void streamdeck::updateButtonBoxes() {
    for (int i = 0; i < num_buttons; ++i) {
        buttonBoxes[i]->clear();
        for (const auto& button : buttonMaps) {
            buttonBoxes[i]->addItem(button.name);
        }
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
        chosenMaps[buttonBoxIndex] = buttonMaps[index].keyCode;
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

