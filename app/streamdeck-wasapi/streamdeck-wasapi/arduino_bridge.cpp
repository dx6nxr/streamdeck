// This file is handling the retrieval of information from the Arduino board.
// It uses the Serial library to communicate with the board and retrieve information about the connected devices.

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread> // Only needed if running io_context in a separate thread
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp> // If using older Boost versions for placeholders
#include "arduino_bridge.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <tchar.h>

#pragma comment(lib, "setupapi.lib")

// --- Configuration ---
const std::string SERIAL_PORT_NAME = "COM3"; // Or "/dev/ttyACM0", "/dev/ttyUSB0" etc. - FIND YOURS!
const unsigned int BAUD_RATE = 115200;       // MUST match Arduino's Serial.begin() rate

// --- Global Data (Needs proper synchronization if accessed from multiple threads) ---
// For simplicity, we'll just print. In a real app, store these safely.
std::vector<int> sliderValues(EXPECTED_SLIDERS, 0);
std::vector<int> buttonStates(EXPECTED_BUTTONS, 0);
// std::mutex dataMutex; // Example: Use a mutex if main thread also accesses this data

// --- Asio Components ---
boost::asio::io_context io_ctx;
boost::asio::serial_port serial(io_ctx);
boost::asio::streambuf read_buffer; // Buffer to hold incoming data

// --- Function Declarations ---
void handle_receive(const boost::system::error_code& ec, std::size_t bytes_transferred);
void start_async_read();
void process_data(const std::string& line);

// --- Main Application Logic (Example) ---
void run_application_logic() {
    // This is where your main application code would run (GUI, game loop, etc.)
    // It should NOT block on serial reads.
    // Occasionally, it might read the latest data (using mutex protection if needed)
    while (true) {
        // Example: Print latest values periodically from the main thread
        {
            // std::lock_guard<std::mutex> lock(dataMutex); // Lock if accessing from multiple threads
            std::cout << "Main Loop - Current Sliders: ";
            for (int val : sliderValues) std::cout << val << " ";
            std::cout << " | Buttons: ";
            for (int state : buttonStates) std::cout << state << " ";
            std::cout << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Simulate work
    }
}

// --- Serial Port retireval function ---
std::vector<std::string> GetAvailableCOMPorts() {
    std::vector<std::string> portNames;

    // Setup device information set for all COM ports
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PORTS,     // COM ports device class GUID
        NULL,                     // No enumerator
        NULL,                     // No parent window
        DIGCF_PRESENT             // Only devices present in the system
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get device information set. Error: " << GetLastError() << std::endl;
        return portNames;
    }

    // Enumerate through all devices in the set
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        // Get the registry key containing the COM port name
        HKEY hDeviceKey = SetupDiOpenDevRegKey(
            hDevInfo,
            &devInfoData,
            DICS_FLAG_GLOBAL,
            0,
            DIREG_DEV,            // Device registry branch
            KEY_READ              // Access rights
        );

        if (hDeviceKey != INVALID_HANDLE_VALUE) {
            // Query for the COM port name
            TCHAR portName[64];
            DWORD portNameSize = sizeof(portName);
            DWORD portNameType = 0;

            if (RegQueryValueEx(
                hDeviceKey,
                _T("PortName"),   // Value name
                NULL,             // Reserved
                &portNameType,    // Type
                (LPBYTE)portName, // Data buffer
                &portNameSize     // Buffer size
            ) == ERROR_SUCCESS) {
                // Convert TCHAR to std::string (handles both ANSI and Unicode)
#ifdef UNICODE
                std::wstring wPortName(portName);
                std::string portNameStr(wPortName.begin(), wPortName.end());
#else
                std::string portNameStr(portName);
#endif

                portNames.push_back(portNameStr);
            }

            // Close the registry key
            RegCloseKey(hDeviceKey);
        }
    }

    // Clean up device information set
    SetupDiDestroyDeviceInfoList(hDevInfo);

    return portNames;
}

// --- Serial Handling ---
void process_data(const std::string& line) {
    // std::cout << "Received raw: " << line << std::endl; // Debugging
    std::stringstream ss(line);
    std::string segment;
    std::vector<int> received_values;

    try {
        while (std::getline(ss, segment, ',')) {
            received_values.push_back(std::stoi(segment));
        }

        if (received_values.size() == (EXPECTED_SLIDERS + EXPECTED_BUTTONS)) {
            // Data seems valid, update global state
            // Consider using a mutex here if the main application thread
            // reads this data concurrently.
            // std::lock_guard<std::mutex> lock(dataMutex);
            for (int i = 0; i < EXPECTED_SLIDERS; ++i) {
                sliderValues[i] = received_values[i];
            }
            for (int i = 0; i < EXPECTED_BUTTONS; ++i) {
                buttonStates[i] = received_values[EXPECTED_SLIDERS + i];
            }
            // Optionally signal the main thread that new data is available
            std::cout << "Processed - Sliders: ";
            for (int i = 0; i < EXPECTED_SLIDERS; ++i) std::cout << sliderValues[i] << " ";
            std::cout << "| Buttons: ";
            for (int i = 0; i < EXPECTED_BUTTONS; ++i) std::cout << buttonStates[i] << " ";
            std::cout << std::endl;

        }
        else {
            std::cerr << "Warning: Received line with incorrect number of values ("
                << received_values.size() << "): " << line << std::endl;
        }
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Warning: Could not parse value in line: " << line << " (" << e.what() << ")" << std::endl;
    }
    catch (const std::out_of_range& e) {
        std::cerr << "Warning: Value out of range in line: " << line << " (" << e.what() << ")" << std::endl;
    }
}