#include "backend_logic.hpp" // Include the header file we just created

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream> // For reading file into string easily
#include <optional>
// No need to include <string>, <mutex>, json.hpp, crow.h again (included via backend_logic.hpp)

// --- Define Global Constants ---
// Define the actual constants declared as extern in the header
const std::string CONFIG_FILE = "config.json";
const std::string BINDS_FILE = "binds.json";
const int SERVER_PORT = 8080; // Backend server port

// --- Define Global Mutexes ---
// Define the actual mutex objects declared as extern in the header
std::mutex config_mutex;
std::mutex binds_mutex;

// --- Function Definitions ---

json readJsonFile(const std::string &filename, std::mutex &file_mutex)
{
    json data;

    try
    {
        std::lock_guard<std::mutex> lock(file_mutex);

        std::ifstream input_file(filename);
        if (!input_file.is_open())
        {
            std::cerr << "Warning: Could not open file " << filename << std::endl;
            return filename == BINDS_FILE ? json::array() : json::object();
        }

        // Check if file is empty
        input_file.seekg(0, std::ios::end);
        if (input_file.tellg() == 0)
        {
            std::cerr << "Warning: File " << filename << " is empty" << std::endl;
            // Return empty array for binds.json, empty object for config.json
            return filename == BINDS_FILE ? json::array() : json::object();
        }

        // Reset file position to beginning
        input_file.seekg(0, std::ios::beg);

        try
        {
            input_file >> data;
        }
        catch (json::parse_error &e)
        {
            std::cerr << "Error: Failed to parse JSON file " << filename << ". Error: " << e.what() << std::endl;
            // Return empty array for binds.json, empty object for config.json
            return filename == BINDS_FILE ? json::array() : json::object();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in readJsonFile: " << e.what() << std::endl;
        // Return empty array for binds.json, empty object for config.json
        return filename == BINDS_FILE ? json::array() : json::object();
    }

    // Validate the data structure: binds.json should be an array, config.json should be an object
    if (filename == BINDS_FILE && !data.is_array())
    {
        std::cerr << "Warning: " << filename << " does not contain a valid array, resetting" << std::endl;
        return json::array();
    }
    else if (filename == CONFIG_FILE && !data.is_object())
    {
        std::cerr << "Warning: " << filename << " does not contain a valid object, resetting" << std::endl;
        return json::object();
    }

    return data;
}

bool writeJsonFile(const std::string &filename, const json &data, std::mutex &file_mutex)
{
    std::lock_guard<std::mutex> lock(file_mutex);
    std::ofstream file_stream(filename);
    if (!file_stream.is_open())
    {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return false;
    }
    try
    {
        file_stream << data.dump(4); // Pretty-print
        file_stream.close();
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: Failed to write JSON to file " << filename << ". Error: " << e.what() << std::endl;
        file_stream.close();
        return false;
    }
}

void addCorsHeaders(crow::response &res)
{
    // Allow requests from specific origin needed for your frontend
    res.add_header("Access-Control-Allow-Origin", "http://localhost:3000");
    // Specify allowed methods for actual requests
    res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    // Specify allowed headers in actual requests (Content-Type is common)
    res.add_header("Access-Control-Allow-Headers", "Content-Type");
    // Allow credentials if needed (cookies, authorization headers, etc.)
    res.add_header("Access-Control-Allow-Credentials", "true");
}

std::optional<std::string> readFileContent(const std::string &filepath)
{
    std::ifstream file_stream(filepath);
    if (!file_stream.is_open())
    {
        std::cerr << "Warning: Could not open static file: " << filepath << std::endl;
        return std::nullopt; // Indicate failure
    }
    std::stringstream buffer;
    buffer << file_stream.rdbuf(); // Read entire file into the buffer
    file_stream.close();
    return buffer.str(); // Return content as string
}

const char *getMimeType(const std::string &filepath)
{
    size_t dot_pos = filepath.rfind('.');
    if (dot_pos == std::string::npos)
    {
        return "application/octet-stream"; // Default binary type
    }

    std::string ext = filepath.substr(dot_pos + 1);

    if (ext == "html" || ext == "htm")
        return "text/html";
    if (ext == "css")
        return "text/css";
    if (ext == "js")
        return "application/javascript";
    if (ext == "png")
        return "image/png";
    if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    if (ext == "gif")
        return "image/gif";
    if (ext == "svg")
        return "image/svg+xml";
    if (ext == "ico")
        return "image/x-icon";
    // Add more MIME types as needed

    return "application/octet-stream"; // Default
}

// Get virtual key code from key name
int getVirtualKeyCode(const std::string &keyName)
{
    // Common keys
    if (keyName == "A")
        return 'A';
    if (keyName == "B")
        return 'B';
    if (keyName == "C")
        return 'C';
    // Add remaining letters...
    if (keyName == "Z")
        return 'Z';

    // Numbers
    if (keyName == "0")
        return '0';
    if (keyName == "1")
        return '1';
    // Add remaining numbers...
    if (keyName == "9")
        return '9';

    // Function keys
    if (keyName == "F1")
        return VK_F1;
    if (keyName == "F2")
        return VK_F2;
    // Add remaining function keys...
    if (keyName == "F12")
        return VK_F12;

    // Special keys
    if (keyName == "Escape" || keyName == "Esc")
        return VK_ESCAPE;
    if (keyName == "Tab")
        return VK_TAB;
    if (keyName == "CapsLock")
        return VK_CAPITAL;
    if (keyName == "Shift")
        return VK_SHIFT;
    if (keyName == "Control" || keyName == "Ctrl")
        return VK_CONTROL;
    if (keyName == "Alt")
        return VK_MENU;
    if (keyName == "Space")
        return VK_SPACE;
    if (keyName == "Enter")
        return VK_RETURN;
    if (keyName == "Backspace")
        return VK_BACK;

    // Arrow keys
    if (keyName == "ArrowUp" || keyName == "Up")
        return VK_UP;
    if (keyName == "ArrowDown" || keyName == "Down")
        return VK_DOWN;
    if (keyName == "ArrowLeft" || keyName == "Left")
        return VK_LEFT;
    if (keyName == "ArrowRight" || keyName == "Right")
        return VK_RIGHT;

    // Multimedia keys
    if (keyName == "Media_PlayPause")
        return VK_MEDIA_PLAY_PAUSE;
    if (keyName == "Media_NextTrack")
        return VK_MEDIA_NEXT_TRACK;
    if (keyName == "Media_PrevTrack")
        return VK_MEDIA_PREV_TRACK;
    if (keyName == "Media_Stop")
        return VK_MEDIA_STOP;
    if (keyName == "Media_VolumeUp")
        return VK_VOLUME_UP;
    if (keyName == "Media_VolumeDown")
        return VK_VOLUME_DOWN;
    if (keyName == "Media_VolumeMute")
        return VK_VOLUME_MUTE;

    // Default: unknown key
    std::cerr << "Unknown key name: " << keyName << std::endl;
    return 0;
}

// Simulate a hotkey combination (key code + modifiers)
void simulateHotkey(int keyCode, int modifiers)
{
    try
    {
        std::cout << "Simulating hotkey with keyCode: 0x" << std::hex << keyCode
                  << std::dec << " and modifiers: " << modifiers << std::endl;

        // Array to hold all inputs (modifiers + main key)
        INPUT inputs[4] = {};
        int inputCount = 0;

        // Add modifier keys if needed
        if (modifiers & 1)
        { // Ctrl
            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_CONTROL;
            inputCount++;
        }
        if (modifiers & 2)
        { // Shift
            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_SHIFT;
            inputCount++;
        }
        if (modifiers & 4)
        { // Alt
            inputs[inputCount].type = INPUT_KEYBOARD;
            inputs[inputCount].ki.wVk = VK_MENU;
            inputCount++;
        }

        // Main key
        inputs[inputCount].type = INPUT_KEYBOARD;
        inputs[inputCount].ki.wVk = keyCode;
        inputCount++;

        // Send key press events
        UINT uSent = SendInput(inputCount, inputs, sizeof(INPUT));
        if (uSent != inputCount)
        {
            std::cerr << "SendInput failed: " << GetLastError() << std::endl;
        }
        else
        {
            std::cout << "Key press sent successfully" << std::endl;
        }

        // Small delay
        Sleep(50);

        // Release all keys (in reverse order)
        for (int i = 0; i < inputCount; i++)
        {
            inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
        }

        // Send key release events
        uSent = SendInput(inputCount, inputs, sizeof(INPUT));
        if (uSent != inputCount)
        {
            std::cerr << "SendInput (release) failed: " << GetLastError() << std::endl;
        }
        else
        {
            std::cout << "Key release sent successfully" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in simulateHotkey: " << e.what() << std::endl;
    }
}

// Simulate a multimedia key press specifically
bool simulateMediaKey(int mediaKey)
{
    try
    {
        std::cout << "Simulating media key with code: 0x" << std::hex << mediaKey << std::dec << std::endl;

        // Press key
        keybd_event(mediaKey, 0xbf, 0, 0);

        // Add a small delay
        Sleep(50);

        // Release key
        keybd_event(mediaKey, 0xbf, KEYEVENTF_KEYUP, 0);

        std::cout << "Media key simulation sent." << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in simulateMediaKey: " << e.what() << std::endl;
        return false;
    }
}
