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

// Callback function when data is received (up to the delimiter '\n')
void handle_receive(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "Error receiving data: " << ec.message() << std::endl;
        // Handle error (e.g., port closed, device disconnected)
        // Maybe try reopening the port after a delay?
        serial.close(); // Close the port on error
        // Add logic here to attempt reconnection if desired
        return;
    }

    if (bytes_transferred > 0) {
        // Convert the received data in the buffer to a string
        std::istream is(&read_buffer);
        std::string line;
        std::getline(is, line); // Reads up to the delimiter '\n'

        // Remove potential carriage return '\r' sent by Arduino's println
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            process_data(line);
        }
    }

    // After handling the current data, immediately start listening for the next line
    start_async_read();
}

// Initiates an asynchronous read operation
void start_async_read() {
    // Read until a newline character ('\n') is encountered
    boost::asio::async_read_until(serial, read_buffer, '\n',
        boost::bind(&handle_receive, // Use boost::bind for older Boost/compilers
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    // C++11 lambda alternative (often preferred):
    /*
    boost::asio::async_read_until(serial, read_buffer, '\n',
        [](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            handle_receive(ec, bytes_transferred);
        });
    */
}


int main() {
    try {
        // --- Open and Configure Serial Port ---
        serial.open(SERIAL_PORT_NAME);
        serial.set_option(boost::asio::serial_port_base::baud_rate(BAUD_RATE));
        serial.set_option(boost::asio::serial_port_base::character_size(8));
        serial.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
        serial.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
        serial.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

        std::cout << "Serial port " << SERIAL_PORT_NAME << " opened successfully at " << BAUD_RATE << " baud." << std::endl;

        // --- Start Reading ---
        start_async_read(); // Start the first asynchronous read

        std::cout << "Starting Asio io_context..." << std::endl;

        // --- Run the Asio event loop ---
        // Option 1: Run io_context in the main thread.
        // Your application logic needs to be integrated differently or run elsewhere.
        // io_ctx.run(); // This call blocks until io_ctx.stop() is called or work runs out

        // Option 2: Run io_context in a separate thread (More common for GUI/Game loops)
        std::thread asio_thread([&]() {
            try {
                io_ctx.run();
            }
            catch (const std::exception& e) {
                std::cerr << "Exception in ASIO thread: " << e.what() << std::endl;
            }
            });
        std::cout << "ASIO thread started." << std::endl;

        // Run your main application logic here (non-blocking)
        run_application_logic(); // This function would contain your app's main loop

        // --- Cleanup ---
        asio_thread.join(); // Wait for the ASIO thread to finish (if using Option 2)


    }
    catch (const boost::system::system_error& e) {
        std::cerr << "Error opening or configuring serial port " << SERIAL_PORT_NAME << ": " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Program finished." << std::endl;
    return 0;
}