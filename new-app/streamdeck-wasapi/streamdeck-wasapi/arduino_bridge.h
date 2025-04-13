#pragma once
#ifndef ARDUINO_BRIDGE_H
#define ARDUINO_BRIDGE_H

#include <vector>
#include <string>
#include <mutex> // Recommended if adding thread-safe getters

// This header file declares the public interface for retrieving information
// from an Arduino board via asynchronous serial communication using Boost.Asio.

// --- Configuration Constants ---
// These define the expected number of inputs from the Arduino.
// Ensure these match the definitions in the corresponding .cpp file
// and the Arduino sketch.
constexpr int EXPECTED_SLIDERS = 4;
constexpr int EXPECTED_BUTTONS = 8;

// --- Public Data Access ---
// Provides access to the latest values read from the Arduino.
// NOTE: These variables are defined in the corresponding .cpp file.
// WARNING: Accessing these directly from multiple threads without
// external synchronization (like a mutex) is NOT thread-safe if the
// main application thread reads while the ASIO thread writes.
// Consider using thread-safe getter functions instead.
extern std::vector<int> sliderValues;
extern std::vector<int> buttonStates;

// --- Optional: Mutex for thread safety (Recommended) ---
// If you want to provide thread-safe access, declare the mutex here
// and define it in the .cpp file. Use std::lock_guard in getter functions.
// extern std::mutex dataMutex;

// --- Control Functions ---

/**
 * @brief Initializes and starts the serial communication listener.
 * Opens the specified serial port, configures it according to predefined
 * settings (baud rate, data bits, etc.), and starts an asynchronous
 * read loop managed by Boost.Asio, typically running in a separate thread.
 *
 * @param port_name The platform-specific name of the serial port
 * (e.g., "COM3" on Windows, "/dev/ttyACM0" on Linux).
 * @param baud_rate The communication speed (bits per second), must match the
 * Arduino's configuration (e.g., 115200).
 * @return true if the port was opened and the reader thread started successfully,
 * false otherwise (e.g., port not found, permissions error).
 */
bool start_arduino_reader(const std::string& port_name, unsigned int baud_rate);

/**
 * @brief Stops the serial communication listener thread gracefully.
 * Signals the Boost.Asio event loop to stop processing, closes the
 * serial port, and waits for the reader thread to complete its execution.
 * This should be called before the application exits to ensure clean shutdown.
 */
void stop_arduino_reader();


// --- Optional: Thread-safe Getter Function Declarations ---
// Uncomment these and implement them in the .cpp file using the mutex.
/*
std::vector<int> get_latest_slider_values();
std::vector<int> get_latest_button_states();
*/

#endif // ARDUINO_READER_HPP