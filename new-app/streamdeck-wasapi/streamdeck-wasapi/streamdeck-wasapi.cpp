#include "framework.h"
// Prevent Windows from defining min and max macros
#define NOMINMAX
#include "streamdeck-wasapi.h"
#include "resource.h"
#include <iostream>
#include <vector>
#include <shellapi.h>
#include <thread>
#include <atomic>
#include <memory>
#include "backend_logic.hpp"
#include "arduino_bridge.h"
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;

#define MAX_LOADSTRING 100
#define SAFE_RELEASE(ptr) \
    if (ptr)              \
    {                     \
        (ptr)->Release(); \
        ptr = nullptr;    \
    }

// Arduino configuration constants
std::string SERIAL_PORT_NAME = "";     // Empty by default - user must select a valid port
const unsigned int BAUD_RATE = 115200; // Default baud rate for Arduino communication

// Arduino connection status
bool g_arduino_connected = false;

// Create new instances of io_context and serial_port to avoid reuse issues
boost::asio::io_context *create_io_context()
{
    return new boost::asio::io_context();
}

boost::asio::serial_port *create_serial_port(boost::asio::io_context &io_ctx)
{
    return new boost::asio::serial_port(io_ctx);
}

// Global Variables
HINSTANCE hInst = nullptr;
HWND g_hwnd = nullptr;
WCHAR szTitle[MAX_LOADSTRING] = {0};
WCHAR szWindowClass[MAX_LOADSTRING] = {0};

// Server-related globals
crow::SimpleApp g_crow_app;
std::unique_ptr<std::thread> g_server_thread;
std::atomic<bool> g_server_running(false);

// Arduino-related globals
std::unique_ptr<std::thread> g_arduino_thread;
std::atomic<bool> g_arduino_running(false);
std::mutex arduino_data_mutex;
std::vector<int> g_slider_values(EXPECTED_SLIDERS, 0);
std::vector<int> g_button_states(EXPECTED_BUTTONS, 0);

// ASIO globals (replace the external declarations)
std::unique_ptr<boost::asio::io_context> io_ctx;
std::unique_ptr<boost::asio::serial_port> serial;
static boost::asio::streambuf read_buffer; // Buffer to hold incoming data - static to avoid multiple definition

// External function declarations
extern void AddTrayIcon(HWND hwnd, HINSTANCE hinstance, LPCWSTR tip);
extern void RemoveTrayIcon(HWND hwnd);
extern void HandleTrayIconClick(HWND hwnd, LPARAM lParam);
extern bool InitializeWasapi();
extern void SetApplicationVolume(const std::wstring &appName, float volume);
extern std::vector<std::wstring> GetApplicationNames();
extern void ToggleMuteApplication(const std::wstring &appName);
extern void RefreshAudioSessions();
extern void ShowTrayBalloonTip(const wchar_t *title, const wchar_t *message, DWORD infoFlags);
extern bool g_wasapiInitialized;
extern std::string ws2s(const std::wstring &wstr);

// Forward declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void StartWebServer();
void StartArduinoMonitor();
void ProcessArduinoData();
void ApplyVolumeToGroup(const std::string &group_name, float volume);
void HandleButtonPress(int button_index);

// Add this constant near other constants at the top
const std::string SHADCN_UI_PATH = "./shadcn-ui/.next/static";
const std::string SHADCN_UI_SERVER_PATH = "/static";
const std::string PUBLIC_PATH = "./public";
const std::string CSS_PATH = "/css";

// Function to clamp a value between min and max
template <typename T>
T clamp(T value, T min, T max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine,
                    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings - check for errors
    if (!LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING) ||
        !LoadStringW(hInstance, IDC_STREAMDECKWASAPI, szWindowClass, MAX_LOADSTRING) ||
        !MyRegisterClass(hInstance) ||
        !InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    const HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_STREAMDECKWASAPI));

    // Main message loop
    MSG msg = {0};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {0};

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_STREAMDECKWASAPI));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

void StartWebServer()
{
    std::cout << "Web server thread started." << std::endl;
    g_server_running = true;

    // Set up CORS options handler
    auto options_handler = [](const crow::request & /*req*/, crow::response &res)
    {
        addCorsHeaders(res);
        res.code = 204;
        res.end();
    };

    // Static file routes
    CROW_ROUTE(g_crow_app, "/")([]()
                                {
        auto result = readFileContent("public/index.html");
        if (result.has_value()) {
            crow::response res;
            addCorsHeaders(res);
            res.add_header("Content-Type", getMimeType("public/index.html"));
            res.write(result.value());
            return res;
        }
        return crow::response(404, "Not Found: index.html"); });

    CROW_ROUTE(g_crow_app, "/style.css")([]()
                                         {
        auto result = readFileContent("public/style.css");
        if (result.has_value()) {
            crow::response res;
            addCorsHeaders(res);
            res.add_header("Content-Type", getMimeType("public/style.css"));
            res.write(result.value());
            return res;
        }
        return crow::response(404, "Not Found: style.css"); });

    CROW_ROUTE(g_crow_app, "/material-you.css")([]()
                                                {
        auto result = readFileContent("public/material-you.css");
        if (result.has_value()) {
            crow::response res;
            addCorsHeaders(res);
            res.add_header("Content-Type", getMimeType("public/material-you.css"));
            res.write(result.value());
            return res;
        }
        return crow::response(404, "Not Found: material-you.css"); });

    CROW_ROUTE(g_crow_app, "/script.js")([]()
                                         {
        auto result = readFileContent("public/script.js");
        if (result.has_value()) {
            crow::response res;
            addCorsHeaders(res);
            res.add_header("Content-Type", getMimeType("public/script.js"));
            res.write(result.value());
            return res;
        }
        return crow::response(404, "Not Found: script.js"); });

    // Define OPTIONS routes
    CROW_ROUTE(g_crow_app, "/api/save-config").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/save-binds").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/get-apps").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/load-config").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/load-binds").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/get-controller-state").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/get-com-ports").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/set-com-port").methods("OPTIONS"_method)(options_handler);
    CROW_ROUTE(g_crow_app, "/api/test-volume").methods("OPTIONS"_method)(options_handler);

    // GET /api/load-config
    CROW_ROUTE(g_crow_app, "/api/load-config").methods("GET"_method)([](const crow::request & /*req*/, crow::response &res)
                                                                     {
        std::cout << "API: GET /api/load-config" << std::endl;
        json config_data = readJsonFile(CONFIG_FILE, config_mutex);
        
        // Debug output the loaded config
        std::cout << "Loaded config from file: " << std::endl;
        if (config_data.contains("groups")) {
            std::cout << "  - Contains 'groups' with " << config_data["groups"].size() << " entries" << std::endl;
        } else {
            std::cout << "  - Missing 'groups' field" << std::endl;
        }
        
        if (config_data.contains("group_names")) {
            std::cout << "  - Contains 'group_names' with " << config_data["group_names"].size() << " entries:" << std::endl;
            // Print each group name mapping
            for (auto& [key, value] : config_data["group_names"].items()) {
                std::cout << "    * '" << key << "' -> '" << value << "'" << std::endl;
            }
        } else {
            std::cout << "  - Missing 'group_names' field, will initialize it" << std::endl;
        }
        
        // Ensure group_names is present in the config
        if (!config_data.contains("group_names")) {
            config_data["group_names"] = json::object();
            
            // Initialize with default names based on existing groups
            if (config_data.contains("groups") && config_data["groups"].is_object()) {
                for (auto& [name, group] : config_data["groups"].items()) {
                    if (!config_data["group_names"].contains(name)) {
                        config_data["group_names"][name] = name;
                        std::cout << "    - Created default name mapping: '" << name << "' -> '" << name << "'" << std::endl;
                    }
                }
            }
            
            // Save the updated config with group_names
            std::cout << "  - Saving updated config with new group_names field" << std::endl;
            writeJsonFile(CONFIG_FILE, config_data, config_mutex);
        }

        // Ensure buttonBindings is present in the config
        if (!config_data.contains("buttonBindings"))
        {
            std::cout << "  - Missing 'buttonBindings' field, will initialize it" << std::endl;
            config_data["buttonBindings"] = json::object();
            writeJsonFile(CONFIG_FILE, config_data, config_mutex);
        }

        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        res.write(config_data.dump());
        res.end(); });

    // POST /api/save-config
    CROW_ROUTE(g_crow_app, "/api/save-config").methods("POST"_method)([](const crow::request &req, crow::response &res)
                                                                      {
        std::cout << "API: POST /api/save-config" << std::endl;
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        json config_data;
        try {
            config_data = json::parse(req.body);
        }
        catch (...) {
            res.code = 400;
            res.write("{\"error\":\"Invalid JSON\"}");
            res.end();
            return;
        }
        
        // Ensure the required fields exist
        if (!config_data.contains("groups") || !config_data["groups"].is_object()) {
            res.code = 400;
            res.write("{\"error\":\"Missing or invalid 'groups' field\"}");
            res.end();
            return;
        }
        
        // Ensure group_names is present and is an object
        if (!config_data.contains("group_names")) {
            config_data["group_names"] = json::object();
        }
        else if (!config_data["group_names"].is_object()) {
            res.code = 400;
            res.write("{'error':'Invalid group_names field - must be an object'}");
            res.end();
            return;
        }

        // Ensure buttonBindings is present and is an object
        if (config_data.contains("buttonBindings")) {
            if (!config_data["buttonBindings"].is_object()) {
                res.code = 400;
                res.write("{'error':'Invalid buttonBindings field - must be an object'}");
                res.end();
                return;
            }
        }
        else
        {
            // If it doesn't exist, create an empty one to ensure consistency
            config_data["buttonBindings"] = json::object();
        }

        // Ensure all groups have an entry in group_names
        for (auto& [name, group] : config_data["groups"].items()) {
            if (!config_data["group_names"].contains(name)) {
                // Use the group key as the default display name
                config_data["group_names"][name] = name;
            }
        }
        
        // Remove any group_names entries for non-existent groups
        std::vector<std::string> keysToRemove;
        for (auto& [name, displayName] : config_data["group_names"].items()) {
            if (!config_data["groups"].contains(name)) {
                keysToRemove.push_back(name);
            }
        }
        
        for (const auto& key : keysToRemove) {
            config_data["group_names"].erase(key);
        }
        
        if (writeJsonFile(CONFIG_FILE, config_data, config_mutex)) {
            res.code = 200;
            res.write("{\"message\":\"Config saved\"}");
        }
        else {
            res.code = 500;
            res.write("{\"error\":\"Failed write config\"}");
        }
        res.end(); });

    // GET /api/load-binds
    CROW_ROUTE(g_crow_app, "/api/load-binds").methods("GET"_method)([](const crow::request & /*req*/, crow::response &res)
                                                                    {
        std::cout << "API: GET /api/load-binds" << std::endl;
        json binds_data = readJsonFile(BINDS_FILE, binds_mutex);
        if (!binds_data.is_array()) {
            binds_data = json::array();
        }
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        res.write(binds_data.dump());
        res.end(); });

    // POST /api/save-binds
    CROW_ROUTE(g_crow_app, "/api/save-binds").methods("POST"_method)([](const crow::request &req, crow::response &res)
                                                                     {
        std::cout << "API: POST /api/save-binds" << std::endl;
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        json binds_data;
        try {
            binds_data = json::parse(req.body);
            if (!binds_data.is_array())
                throw std::runtime_error("Not an array");
        }
        catch (...) {
            res.code = 400;
            res.write("{\"error\":\"Invalid JSON array\"}");
            res.end();
            return;
        }

        if (writeJsonFile(BINDS_FILE, binds_data, binds_mutex)) {
            res.code = 200;
            res.write("{\"message\":\"Bindings saved\"}");
        }
        else {
            res.code = 500;
            res.write("{\"error\":\"Failed write bindings\"}");
        }
        res.end(); });

    // POST /api/get-apps
    CROW_ROUTE(g_crow_app, "/api/get-apps").methods("POST"_method)([](const crow::request & /*req*/, crow::response &res)
                                                                   {
        std::cout << "API: POST /api/get-apps" << std::endl;

        RefreshAudioSessions();
        std::vector<std::wstring> appNamesW = GetApplicationNames();
        std::vector<std::string> appNamesA;

        appNamesA.reserve(appNamesW.size()); // Pre-allocate memory

        for (const auto& wname : appNamesW) {
            if (wname.empty()) continue;

            int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len <= 0) continue;

            std::string name(len - 1, 0);
            if (WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &name[0], len, nullptr, nullptr) <= 0) {
                continue;
            }

            appNamesA.push_back(name);
        }

        json app_list = appNamesA;
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        res.write(app_list.dump());
        res.end(); });

    // GET /api/get-controller-state - Update to match the new UI's expected format
    CROW_ROUTE(g_crow_app, "/api/get-controller-state").methods("GET"_method)([](const crow::request & /*req*/, crow::response &res)
                                                                              {
        // std::cout << "API: GET /api/get-controller-state" << std::endl;
        
        json state_data = json::object();
        
        // Get the current Arduino slider and button values
        {
            std::lock_guard<std::mutex> lock(arduino_data_mutex);
            json sliders = json::array();
            json buttons = json::array();
            
            // Format sliders for the shadcn UI
            for (size_t i = 0; i < g_slider_values.size(); i++) {
                sliders.push_back({
                    {"id", static_cast<int>(i) + 1},
                    {"value", g_slider_values[i]},
                    {"label", "Slider " + std::to_string(i + 1)}
                });
            }
            
            // Format buttons for the shadcn UI
            for (size_t i = 0; i < g_button_states.size(); i++) {
                buttons.push_back({
                    {"id", static_cast<int>(i) + 1},
                    {"pressed", g_button_states[i] > 0},
                    {"label", "Button " + std::to_string(i + 1)}
                });
            }
            
            state_data["sliders"] = sliders;
            state_data["buttons"] = buttons;
            state_data["connected"] = g_arduino_connected;
            state_data["port"] = SERIAL_PORT_NAME;
        }
        
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        res.write(state_data.dump());
        res.end(); });

    // GET /api/get-com-ports - Update to match the new UI's expected format
    CROW_ROUTE(g_crow_app, "/api/get-com-ports").methods("GET"_method)([](const crow::request & /*req*/, crow::response &res)
                                                                       {
        std::cout << "API: GET /api/get-com-ports" << std::endl;
        
        // Get list of available COM ports
        std::vector<std::string> comPorts = GetAvailableCOMPorts();
        
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        res.write(json(comPorts).dump());  // Just return the array directly
		// print the json(comPorts) to the console for debugging
		std::cout << "Available COM ports: " << json(comPorts).dump() << std::endl;
        res.end(); });

    // POST /api/set-com-port
    CROW_ROUTE(g_crow_app, "/api/set-com-port").methods("POST"_method)([](const crow::request &req, crow::response &res)
                                                                       {
        std::cout << "API: POST /api/set-com-port" << std::endl;
        
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        
        json request_data;
        try {
            request_data = json::parse(req.body);
        }
        catch (...) {
            res.code = 400;
            res.write("{\"error\":\"Invalid JSON\"}");
            res.end();
            return;
        }
        
        if (!request_data.contains("port") || !request_data["port"].is_string()) {
            res.code = 400;
            res.write("{\"error\":\"Missing or invalid port parameter\"}");
            res.end();
            return;
        }
        
        // Get the new port from the request
        std::string newPort = request_data["port"];
        std::cout << "Setting COM port to: " << newPort << std::endl;
        
        // Check if we need to restart the Arduino connection
        bool needRestart = (g_arduino_running && SERIAL_PORT_NAME != newPort);
        bool wasEmpty = SERIAL_PORT_NAME.empty();
        
        // Update the port
        SERIAL_PORT_NAME = newPort;
        
        // If Arduino is running and port changed, restart it
        if (needRestart) {
            // Stop Arduino thread safely - make a copy of the variables we need to modify
            g_arduino_connected = false;
            
            // Make a local copy of the thread pointer to avoid race conditions
            auto thread_ptr = std::move(g_arduino_thread);
            
            // Only try to stop the io_ctx if it exists
            if (io_ctx) {
                try {
                    io_ctx->stop();
                }
                catch (const std::exception& e) {
                    std::cerr << "Exception stopping io_ctx: " << e.what() << std::endl;
                }
            }
            
            // Wait for thread to finish if it's joinable
            if (thread_ptr && thread_ptr->joinable()) {
                try {
                    thread_ptr->join();
                    std::cout << "Arduino thread stopped successfully" << std::endl;
                }
                catch (const std::exception& e) {
                    std::cerr << "Exception joining Arduino thread: " << e.what() << std::endl;
                }
            }
            
            // Allow a moment for resources to be released
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Start new Arduino thread with new port
            try {
                g_arduino_thread = std::make_unique<std::thread>(StartArduinoMonitor);
                std::cout << "New Arduino thread started for port " << newPort << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to restart Arduino thread: " << e.what() << std::endl;
                res.code = 500;
                res.write("{\"error\":\"Failed to restart Arduino with new port\"}");
                res.end();
                return;
            }
        }
        else if (wasEmpty && !newPort.empty()) {
            // Port was previously empty but now we have a valid port - start Arduino thread
            std::cout << "Starting Arduino thread for newly set port: " << newPort << std::endl;
            
            // Ensure we don't have any lingering thread objects
            if (g_arduino_thread && g_arduino_thread->joinable()) {
                try {
                    g_arduino_thread->join();
                }
                catch (const std::exception& e) {
                    std::cerr << "Exception joining existing Arduino thread: " << e.what() << std::endl;
                }
            }
            g_arduino_thread.reset();
            
            // Create a fresh thread
            try {
                g_arduino_thread = std::make_unique<std::thread>(StartArduinoMonitor);
                std::cout << "New Arduino thread started for port " << newPort << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to start Arduino thread: " << e.what() << std::endl;
                res.code = 500;
                res.write("{\"error\":\"Failed to start Arduino with new port\"}");
                res.end();
                return;
            }
        }
        else if (!g_arduino_running && !newPort.empty()) {
            // Arduino thread not running but we have a valid port
            std::cout << "Arduino not running but port set: " << newPort << ", starting thread" << std::endl;
            
            // Ensure we don't have any lingering thread objects
            if (g_arduino_thread && g_arduino_thread->joinable()) {
                try {
                    g_arduino_thread->join();
                }
                catch (const std::exception& e) {
                    std::cerr << "Exception joining existing Arduino thread: " << e.what() << std::endl;
                }
            }
            g_arduino_thread.reset();
            
            // Create a fresh thread
            try {
                // Reset g_arduino_running to true to ensure thread will run
                g_arduino_running = true;
                g_arduino_thread = std::make_unique<std::thread>(StartArduinoMonitor);
                std::cout << "New Arduino thread started for port " << newPort << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to start Arduino thread: " << e.what() << std::endl;
                res.code = 500;
                res.write("{\"error\":\"Failed to start Arduino with new port\"}");
                res.end();
                return;
            }
        }
        
        res.code = 200;
        res.write("{\"message\":\"COM port set to " + SERIAL_PORT_NAME + "\", \"connected\": " + (g_arduino_connected ? "true" : "false") + "}");
        res.end(); });

    std::cout << "Starting Crow server on port " << SERVER_PORT << " in background thread..." << std::endl;

    // Set server timeout for better responsiveness during shutdown
    g_crow_app.timeout(5);
    g_crow_app.port(SERVER_PORT).run();

    std::cout << "Crow server thread finished." << std::endl;
    g_server_running = false;
}

void StartArduinoMonitor()
{
    g_arduino_running = true;
    std::cout << "Arduino monitor thread started." << std::endl;

    // Return immediately if no COM port is selected, but keep g_arduino_running set to true
    if (SERIAL_PORT_NAME.empty())
    {
        std::cerr << "No COM port selected. Please select a COM port in settings." << std::endl;
        g_arduino_connected = false;
        // Note: We are NOT setting g_arduino_running to false here anymore
        // This allows the ProcessArduinoData thread to keep waiting for data
        return;
    }

    try
    {
        // Create fresh IO context and serial port objects
        if (io_ctx)
        {
            // If it exists but is stopped, reset it
            if (io_ctx->stopped())
            {
                io_ctx.reset(create_io_context());
            }
        }
        else
        {
            // Create a new one if it doesn't exist
            io_ctx.reset(create_io_context());
        }

        if (!io_ctx)
        {
            throw std::runtime_error("Failed to create IO context");
        }

        // Create a new serial port
        try
        {
            serial.reset(create_serial_port(*io_ctx));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to create serial port: " << e.what() << std::endl;
            throw;
        }

        if (!serial)
        {
            throw std::runtime_error("Failed to create serial port");
        }

        // Clear any existing data in the read buffer
        {
            std::istream is(&read_buffer);
            std::string unused;
            while (std::getline(is, unused))
            {
            } // Consume existing data
            read_buffer.consume(read_buffer.size()); // Clear the buffer
        }

        // Open and Configure Serial Port
        std::cout << "Attempting to open serial port: " << SERIAL_PORT_NAME << std::endl;
        if (serial->is_open())
        {
            serial->close();
            std::cout << "Closed previously open serial port" << std::endl;
        }

        serial->open(SERIAL_PORT_NAME);
        serial->set_option(boost::asio::serial_port_base::baud_rate(BAUD_RATE));
        serial->set_option(boost::asio::serial_port_base::character_size(8));
        serial->set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
        serial->set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
        serial->set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

        std::cout << "Serial port " << SERIAL_PORT_NAME << " opened successfully at " << BAUD_RATE << " baud." << std::endl;
        g_arduino_connected = true;

        // Start Reading
        try
        {
            std::cout << "Starting async read..." << std::endl;
            start_async_read(); // Start the first asynchronous read
            std::cout << "Async read started successfully" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception during start_async_read: " << e.what() << std::endl;
            g_arduino_connected = false;
            if (serial && serial->is_open())
            {
                serial->close();
            }
            throw;
        }

        // Run the Asio event loop inside a try-catch block
        if (!io_ctx->stopped())
        {
            try
            {
                std::cout << "Starting ASIO io_context run loop..." << std::endl;
                io_ctx->run();
                std::cout << "ASIO io_context run loop ended normally" << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception in io_ctx.run(): " << e.what() << std::endl;
            }
        }
        else
        {
            std::cerr << "Warning: IO context was stopped before run() was called" << std::endl;
        }
    }
    catch (const boost::system::system_error &e)
    {
        std::cerr << "Error opening or configuring serial port: " << e.what() << std::endl;
        g_arduino_connected = false;
        // Keep g_arduino_running true so ProcessArduinoData keeps waiting
    }
    catch (const std::exception &e)
    {
        std::cerr << "An unexpected error in Arduino thread: " << e.what() << std::endl;
        g_arduino_connected = false;
        // Keep g_arduino_running true so ProcessArduinoData keeps waiting
    }

    // Clean up before exiting thread
    try
    {
        if (serial && serial->is_open())
        {
            serial->close();
            std::cout << "Serial port closed on thread exit" << std::endl;
        }
    }
    catch (...)
    {
        std::cerr << "Error closing serial port" << std::endl;
    }

    // Make sure to reset just the connection state flag
    g_arduino_connected = false;
    // Note: We're NOT setting g_arduino_running to false here anymore
    std::cout << "Arduino monitor thread finished, but will remain ready for future connections." << std::endl;
}

void ProcessArduinoData()
{
    // Store previous states to detect changes
    std::vector<int> prevButtonStates(EXPECTED_BUTTONS, 0);
    std::vector<int> prevSliderValues(EXPECTED_SLIDERS, 0);

    // Track if we've ever received data
    bool hasInitialData = false;

    // Counter for "still alive" messages
    int stillAliveCounter = 0;

    // Periodically refresh audio sessions
    int refreshCounter = 0;
    const int REFRESH_INTERVAL = 100; // Refresh every ~5 seconds (100 * 50ms)

    // Output header
    std::cerr << "\n\n==================================================" << std::endl;
    std::cerr << "       ProcessArduinoData Thread Started           " << std::endl;
    std::cerr << "==================================================\n\n"
              << std::endl;

    // Output initial state
    {
        std::lock_guard<std::mutex> lock(arduino_data_mutex);
        std::cerr << "Initial slider values: ";
        for (int val : g_slider_values)
        {
            std::cerr << val << " ";
        }
        std::cerr << std::endl;

        std::cerr << "Initial button states: ";
        for (int val : g_button_states)
        {
            std::cerr << val << " ";
        }
        std::cerr << std::endl;
    }

    // Make sure WASAPI is initialized at the start
    if (!g_wasapiInitialized)
    {
        std::cerr << "Initializing WASAPI from ProcessArduinoData thread..." << std::endl;
        InitializeWasapi();
    }

    std::cerr << "Entering main processing loop, waiting for Arduino data..." << std::endl;
    while (g_arduino_running)
    {
        // Print "still alive" message every 100 iterations (about 5 seconds)
        stillAliveCounter++;
        if (stillAliveCounter >= 100)
        {
            std::cerr << "ProcessArduinoData thread still alive, waiting for data. Arduino connected: "
                      << (g_arduino_connected ? "YES" : "NO") << std::endl;
            stillAliveCounter = 0;
        }

        // Increment refresh counter and check if we need to refresh audio sessions
        refreshCounter++;
        if (refreshCounter >= REFRESH_INTERVAL)
        {
            std::cerr << "Periodic audio session refresh..." << std::endl;
            RefreshAudioSessions();
            refreshCounter = 0;
        }

        // Copy latest values with mutex protection
        std::vector<int> sliderValues;
        std::vector<int> buttonStates;
        bool dataAvailable = false;

        {
            std::lock_guard<std::mutex> lock(arduino_data_mutex);
            if (!g_slider_values.empty() && !g_button_states.empty())
            {
                sliderValues = g_slider_values;
                buttonStates = g_button_states;
                dataAvailable = true;
            }
        }

        // Skip processing if no data is available yet
        if (!dataAvailable)
        {
            // Only print warning every 40 iterations (about 2 seconds) to avoid flooding the console
            if (stillAliveCounter % 40 == 0)
            {
                std::cerr << "WARNING: No Arduino data available yet. Arduino connected: "
                          << (g_arduino_connected ? "YES" : "NO") << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Force output slider values more frequently for debugging
        static int basicOutputCounter = 0;
        basicOutputCounter++;
        if (basicOutputCounter >= 10)
        { // Changed from 20 to 10 for more frequent updates
            std::cerr << "Current slider values: ";
            for (size_t i = 0; i < sliderValues.size(); i++)
            {
                std::cerr << sliderValues[i] << " ";
            }
            std::cerr << std::endl;
            basicOutputCounter = 0;
        }

        try
        {
            // Process slider values (map 0-1023 to 0.0-1.0 float for volume)
            // Only process if this is our first data or sliders have changed
            bool slidersChanged = !hasInitialData;

            // Check if any slider value has changed significantly (more than noise threshold)
            const int SLIDER_CHANGE_THRESHOLD = 0; // To filter out noise in potentiometer readings
            for (size_t i = 0; i < sliderValues.size() && i < prevSliderValues.size(); i++)
            {
                if (std::abs(sliderValues[i] - prevSliderValues[i]) > SLIDER_CHANGE_THRESHOLD)
                {
                    slidersChanged = true;
                    std::cerr << "Slider " << i << " changed significantly: " << prevSliderValues[i] << " -> " << sliderValues[i] << std::endl;
                    break;
                }
            }

            // Set flag that we've received initial data
            if (!hasInitialData)
            {
                hasInitialData = true;
                std::cerr << "Initial slider data received, processing..." << std::endl;
            }

            // Only process slider changes if values have changed
            if (slidersChanged)
            {
                std::cerr << "Sliders changed, reading config file..." << std::endl;
                json config_data = readJsonFile(CONFIG_FILE, config_mutex);
                std::cerr << "Config file read." << std::endl;

                // Dump config to debug
                std::cerr << "Config contents (abbreviated): " << std::endl;
                std::cerr << "Has 'groups': " << (config_data.contains("groups") ? "YES" : "NO") << std::endl;
                if (config_data.contains("groups"))
                {
                    std::cerr << "Groups is object: " << (config_data["groups"].is_object() ? "YES" : "NO") << std::endl;

                    // List all groups
                    std::cerr << "Groups in config: ";
                    for (auto &[name, _] : config_data["groups"].items())
                    {
                        std::cerr << "\"" << name << "\" ";
                    }
                    std::cerr << std::endl;
                }

                // Check if config contains the groups mapping
                if (config_data.contains("groups") && config_data["groups"].is_object())
                {
                    // Get a list of group names to map to sliders
                    std::vector<std::string> groupNames;

                    // Instead of looking for specifically named groups, get all groups
                    // and map them to sliders in the order they appear in the config
                    for (auto &[name, group] : config_data["groups"].items())
                    {
                        // Only add non-empty groups or groups that actually exist
                        if (group.is_array())
                        {
                            groupNames.push_back(name);
                            std::cerr << "Mapping slider to group: \"" << name << "\" with "
                                      << group.size() << " apps" << std::endl;

                            // Enhanced debugging: Show apps in the group
                            std::cerr << "   Apps in group: ";
                            for (const auto &app : group)
                            {
                                if (app.is_string())
                                {
                                    std::cerr << "\"" << app.get<std::string>() << "\" ";
                                }
                            }
                            std::cerr << std::endl;
                        }
                    }

                    std::cerr << "Total mapped groups: " << groupNames.size() << std::endl;

                    // For each slider, map it to a group
                    for (size_t i = 0; i < sliderValues.size() && i < groupNames.size(); i++)
                    {
                        try
                        {
                            // Only apply if this slider has changed significantly
                            if (std::abs(sliderValues[i] - prevSliderValues[i]) > SLIDER_CHANGE_THRESHOLD || !hasInitialData)
                            {
                                float normalized_value = static_cast<float>(sliderValues[i]) / 1023.0f;

                                // Clamp value between a small minimum (to avoid complete silence) and 1.0
                                normalized_value = clamp(normalized_value, 0.0f, 1.0f);

                                const std::string &group_name = groupNames[i];

                                // Debug output
                                std::cerr << "---> Calling ApplyVolumeToGroup for group " << group_name
                                          << " with volume " << normalized_value << std::endl;

                                // Apply volume to all apps in this group
                                ApplyVolumeToGroup(group_name, normalized_value);

                                std::cerr << "<--- Returned from ApplyVolumeToGroup" << std::endl;
                            }
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "Error processing slider " << i << ": " << e.what() << std::endl;
                        }
                    }
                }
                else
                {
                    std::cerr << "Config does not contain properly formatted groups" << std::endl;
                }

                // Save the current slider values for change detection next time
                prevSliderValues = sliderValues;
            }

            // Process button states (0 or 1) - only on rising edge (0->1)
            if (buttonStates.size() == prevButtonStates.size())
            {
                json binds_data = readJsonFile(BINDS_FILE, binds_mutex);

                if (binds_data.is_array())
                {
                    for (size_t i = 0; i < buttonStates.size(); i++)
                    {
                        try
                        {
                            // Only trigger on button press (rising edge: 0->1)
                            if (buttonStates[i] == 1 && prevButtonStates[i] == 0)
                            {
                                std::cerr << "Button " << i << " pressed, calling HandleButtonPress" << std::endl;
                                HandleButtonPress(i);
                            }
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "Error processing button " << i << ": " << e.what() << std::endl;
                        }
                    }
                }

                // Update previous button states for next iteration
                prevButtonStates = buttonStates;
            }
            else
            {
                // Button state array size changed, just copy the new size
                prevButtonStates = buttonStates;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception in ProcessArduinoData: " << e.what() << std::endl;
        }

        // Don't hog the CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cerr << "ProcessArduinoData thread exiting. g_arduino_running = " << g_arduino_running << std::endl;
}

void ApplyVolumeToGroup(const std::string &group_name, float volume)
{
    try
    {
        std::cerr << "\n==================================================" << std::endl;
        std::cerr << "  ApplyVolumeToGroup: " << group_name << " -> " << volume << std::endl;
        std::cerr << "==================================================\n"
                  << std::endl;

        json config_data = readJsonFile(CONFIG_FILE, config_mutex);

        // Check if the group exists in our config
        if (config_data.contains("groups") && config_data["groups"].contains(group_name))
        {
            // Get the apps in this group
            auto &apps = config_data["groups"][group_name];

            std::cout << "Group \"" << group_name << "\" found with "
                      << (apps.is_array() ? apps.size() : 0) << " apps" << std::endl;

            if (apps.is_array())
            {
                if (apps.empty())
                {
                    std::cout << "Group is empty, no apps to adjust volume for" << std::endl;
                    return;
                }

                // Check if we need to refresh sessions
                if (!g_wasapiInitialized)
                {
                    std::cerr << "WASAPI not initialized in ApplyVolumeToGroup, initializing..." << std::endl;
                    InitializeWasapi();
                }

                // Debug output of current audio sessions
                std::cerr << "Current audio sessions:" << std::endl;
                RefreshAudioSessions();
                std::vector<std::wstring>
                    appNames = GetApplicationNames();
                for (size_t i = 0; i < appNames.size(); ++i)
                {
                    std::cerr << "  " << i << ": \"" << ws2s(appNames[i]) << "\"" << std::endl;
                }

                // For each app in the group
                for (const auto &app : apps)
                {
                    if (app.is_string())
                    {
                        std::string app_name = app.get<std::string>();
                        std::cout << "Processing app: \"" << app_name << "\"" << std::endl;

                        // Convert to wide string for WASAPI controller using proper conversion
                        std::wstring w_app_name;
                        try
                        {
                            int size_needed = MultiByteToWideChar(CP_UTF8, 0, app_name.c_str(), -1, NULL, 0);
                            if (size_needed > 0)
                            {
                                w_app_name.resize(size_needed - 1); // Exclude null terminator
                                MultiByteToWideChar(CP_UTF8, 0, app_name.c_str(), -1, &w_app_name[0], size_needed);

                                std::cout << "Converting \"" << app_name << "\" to wide string, calling SetApplicationVolume" << std::endl;

                                // Apply volume to this app
                                SetApplicationVolume(w_app_name, volume);
                            }
                            else
                            {
                                std::cerr << "Error: MultiByteToWideChar returned invalid size for app name: " << app_name << std::endl;
                            }
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "Error: Converting app name '" << app_name << "': " << e.what() << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr << "Error: App entry is not a string" << std::endl;
                    }
                }
            }
            else
            {
                std::cerr << "Error: Group's 'apps' property is not an array" << std::endl;
            }
        }
        else
        {
            std::cerr << "Error: Group \"" << group_name << "\" not found in config" << std::endl;

            // Debug output all available groups
            std::cerr << "Available groups in config:" << std::endl;
            if (config_data.contains("groups") && config_data["groups"].is_object())
            {
                for (auto &[name, group] : config_data["groups"].items())
                {
                    std::cerr << "  - \"" << name << "\" with "
                              << (group.is_array() ? group.size() : 0) << " apps" << std::endl;
                }
            }
            else
            {
                std::cerr << "  No groups found in config" << std::endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in ApplyVolumeToGroup: " << e.what() << std::endl;
    }
}

void HandleButtonPress(int button_index)
{
    try
    {
        std::cout << "Button " << button_index << " pressed, checking for action..." << std::endl;

        // Load config to get button -> action mapping
        json config_data = readJsonFile(CONFIG_FILE, config_mutex);

        // Check if configuration has buttonBindings defined
        if (!config_data.contains("buttonBindings") || !config_data["buttonBindings"].is_object())
        {
            std::cerr << "No buttonBindings defined in config or invalid format" << std::endl;
            return;
        }

        // Find the action for this button
        std::string buttonKey = "button" + std::to_string(button_index);
        if (!config_data["buttonBindings"].contains(buttonKey))
        {
            std::cerr << "No action defined for button " << button_index << std::endl;
            return;
        }

        // Get the action name assigned to this button
        std::string actionName = config_data["buttonBindings"][buttonKey];
        std::cout << "Button " << button_index << " has action: " << actionName << std::endl;

        // Empty action name or special "__none__" value means no action
        if (actionName.empty() || actionName == "__none__")
        {
            std::cerr << "Button " << button_index << " has empty or disabled action" << std::endl;
            return;
        }

        // Load bindings to find the key combo for this action
        json binds_data = readJsonFile(BINDS_FILE, binds_mutex);

        if (!binds_data.is_array())
        {
            std::cerr << "Invalid bindings file format" << std::endl;
            return;
        }

        // Find the binding with the matching action name
        bool found = false;
        for (const auto &binding : binds_data)
        {
            if (binding.contains("action") && binding["action"] == actionName)
            {
                // Found the binding - extract key combo
                if (binding.contains("combo") && binding["combo"].is_string())
                {
                    std::string combo = binding["combo"];
                    std::cout << "Found binding for action '" << actionName
                              << "': combo = '" << combo << "'" << std::endl;

                    // Parse the combo string (e.g., "Ctrl+Alt+S")
                    int modifiers = 0;
                    std::string mainKey;

                    if (combo.find("Ctrl+") != std::string::npos)
                    {
                        modifiers |= 1;
                        combo.replace(combo.find("Ctrl+"), 5, "");
                    }

                    if (combo.find("Shift+") != std::string::npos)
                    {
                        modifiers |= 2;
                        combo.replace(combo.find("Shift+"), 6, "");
                    }

                    if (combo.find("Alt+") != std::string::npos)
                    {
                        modifiers |= 4;
                        combo.replace(combo.find("Alt+"), 4, "");
                    }

                    // The remaining string is the main key
                    mainKey = combo;

                    // Get the virtual key code
                    int keyCode = getVirtualKeyCode(mainKey);
                    if (keyCode == 0)
                    {
                        std::cerr << "Unknown key: " << mainKey << std::endl;
                        return;
                    }

                    std::cout << "Executing parsed hotkey: keyCode=" << keyCode
                              << " with modifiers=" << modifiers << std::endl;

                    // Check if this is a multimedia key
                    bool isMediaKey = false;
                    if (mainKey.find("Media_") == 0)
                    {
                        // This is a media key - use the specialized function
                        std::cout << "Detected multimedia key - using simulateMediaKey" << std::endl;
                        simulateMediaKey(keyCode);
                        isMediaKey = true;
                    }
                    else
                    {
                        // This is a standard key - use the regular function
                        simulateHotkey(keyCode, modifiers);
                    }

                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            std::cerr << "No binding found for action: " << actionName << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in HandleButtonPress: " << e.what() << std::endl;
    }
}

// Modified callback function when data is received from Arduino
void handle_receive(const boost::system::error_code &ec, std::size_t bytes_transferred)
{
    // Check for nullptr before proceeding - this could happen during shutdown
    if (!serial || !io_ctx)
    {
        std::cerr << "Error in handle_receive: serial or io_ctx is null" << std::endl;
        g_arduino_connected = false;
        return;
    }

    try
    {
        if (ec)
        {
            std::cerr << "Error receiving data: " << ec.message() << std::endl;
            if (serial && serial->is_open())
            {
                try
                {
                    serial->close(); // Close the port on error
                    std::cout << "Serial port closed due to error" << std::endl;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Exception closing serial port: " << e.what() << std::endl;
                }
            }
            g_arduino_connected = false;
            return;
        }

        if (bytes_transferred > 0)
        {
            // Convert the received data in the buffer to a string
            std::istream is(&read_buffer);
            std::string line;

            try
            {
                std::getline(is, line); // Reads up to the delimiter '\n'
                std::cout << "Received data: " << line << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception reading from buffer: " << e.what() << std::endl;
                // Continue with empty line so we can at least try to start another read
                line = "";
            }

            // Remove potential carriage return '\r' sent by Arduino's println
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            if (!line.empty())
            {
                std::stringstream ss(line);
                std::string segment;
                std::vector<int> received_values;

                try
                {
                    // Parse comma-separated values
                    while (std::getline(ss, segment, ','))
                    {
                        try
                        {
                            received_values.push_back(std::stoi(segment));
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "Error parsing segment '" << segment << "': " << e.what() << std::endl;
                        }
                    }

                    // Enhanced debugging: Always print values when received
                    std::cout << "DEBUG: Parsed " << received_values.size() << " values from Arduino: ";
                    for (const auto &val : received_values)
                    {
                        std::cout << val << " ";
                    }
                    std::cout << std::endl;

                    if (received_values.size() == (EXPECTED_SLIDERS + EXPECTED_BUTTONS))
                    {
                        // Lock and update global state
                        try
                        {
                            std::lock_guard<std::mutex> lock(arduino_data_mutex);
                            for (int i = 0; i < EXPECTED_SLIDERS && i < static_cast<int>(received_values.size()); ++i)
                            {
                                g_slider_values[i] = received_values[i];
                            }
                            for (int i = 0; i < EXPECTED_BUTTONS && i + EXPECTED_SLIDERS < static_cast<int>(received_values.size()); ++i)
                            {
                                g_button_states[i] = received_values[EXPECTED_SLIDERS + i];
                            }

                            // Enhanced debugging: Always print updated global state
                            std::cout << "DEBUG: Updated global state - Sliders: ";
                            for (const auto &val : g_slider_values)
                            {
                                std::cout << val << " ";
                            }
                            std::cout << " | Buttons: ";
                            for (const auto &val : g_button_states)
                            {
                                std::cout << val << " ";
                            }
                            std::cout << std::endl;
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "Exception updating slider/button values: " << e.what() << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr << "Warning: Received line with incorrect number of values: "
                                  << received_values.size() << " (expected "
                                  << (EXPECTED_SLIDERS + EXPECTED_BUTTONS) << ")" << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Warning: Error parsing Arduino data: " << e.what() << std::endl;
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Unhandled exception in handle_receive: " << e.what() << std::endl;
    }

    // After handling the current data, immediately start listening for the next line
    // Only if we're still connected and running
    if (g_arduino_running && g_arduino_connected && serial && serial->is_open())
    {
        try
        {
            start_async_read();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception in start_async_read after handle_receive: " << e.what() << std::endl;
            g_arduino_connected = false;
        }
    }
}

void start_async_read()
{
    try
    {
        if (!serial || !serial->is_open())
        {
            std::cerr << "Cannot start async read: Serial port is not open" << std::endl;
            g_arduino_connected = false;
            return;
        }

        // Instead of using read_buffer.max_size() which can be very large,
        // we'll just consume any existing data in the buffer to reset it
        if (read_buffer.size() > 0)
        {
            std::istream is(&read_buffer);
            std::string unused;
            while (std::getline(is, unused))
            {
            } // Consume existing data
            read_buffer.consume(read_buffer.size()); // Clear the buffer
        }

        // Read until a newline character ('\n') is encountered with a reasonable buffer size
        boost::asio::async_read_until(*serial, read_buffer, '\n',
                                      [](const boost::system::error_code &ec, std::size_t bytes_transferred)
                                      {
                                          try
                                          {
                                              handle_receive(ec, bytes_transferred);
                                          }
                                          catch (const std::exception &e)
                                          {
                                              std::cerr << "Exception in async_read_until callback: " << e.what() << std::endl;
                                              // Don't rethrow to avoid crashing the ASIO event loop
                                          }
                                      });
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in start_async_read: " << e.what() << std::endl;
        g_arduino_connected = false;

        // Don't rethrow from here - mark the error and let the caller handle
        // the reconnection logic if needed
    }
}

void HideConsole()
{
    ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
}

void ShowConsole()
{
    ::ShowWindow(::GetConsoleWindow(), SW_SHOW);
}

bool IsConsoleVisible()
{
    return ::IsWindowVisible(::GetConsoleWindow()) != FALSE;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    // Setup console with error checking
    if (AllocConsole())
    {
        FILE *pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        freopen_s(&pCout, "CONOUT$", "w", stderr);
        std::cout.clear();
        std::cerr.clear();
    }

    // Create the main window (it can be hidden)
    g_hwnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!g_hwnd)
    {
        return FALSE;
    }

    // To show the main window, change SW_HIDE to SW_SHOWDEFAULT in the line below.
    ShowWindow(g_hwnd, SW_HIDE); // Hide the main window
    UpdateWindow(g_hwnd);

    // This hides the console; to make it visible or check the status, refer to the functions ShowConsole() and IsConsoleVisible()
    HideConsole();

    // Add the tray icon
    AddTrayIcon(g_hwnd, hInst, L"StreamDeck WASAPI");

    // Initialize WASAPI
    if (!InitializeWasapi())
    {
        // Handle initialization failure if needed
    }

    // Start the web server in a background thread
    g_server_thread = std::make_unique<std::thread>(StartWebServer);

    // Start the Arduino monitor thread
    g_arduino_thread = std::make_unique<std::thread>(StartArduinoMonitor);

    // Start the data processing thread
    std::thread(ProcessArduinoData).detach();

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        if (!InitializeWasapi())
        {
            std::cerr << "WASAPI initialization failed" << std::endl;
            MessageBox(hWnd, L"Failed to initialize audio system. The application will now exit.", L"Error", MB_ICONERROR);
            PostQuitMessage(1);
            return -1;
        }
        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);

        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_REFRESH:
            RefreshAudioSessions();
            ShowTrayBalloonTip(L"Sessions Refreshed", L"Audio sessions have been refreshed.", NIIF_INFO);
            break;

        case IDM_OPEN_FRONTEND:
            ShellExecute(NULL, L"open", L"http://localhost:8080", NULL, NULL, SW_SHOWNORMAL);
            break;

        case IDM_SETTINGS:
            // Settings placeholder for future implementation
            break;

        default:
            if (wmId >= IDM_APP_BASE)
            {
                int appIndex = wmId - IDM_APP_BASE;
                std::vector<std::wstring> appNames = GetApplicationNames();
                if (appIndex >= 0 && static_cast<size_t>(appIndex) < appNames.size())
                {
                    ToggleMuteApplication(appNames[appIndex]);
                }
            }
            else
            {
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // Add any drawing code that goes here
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        // Stop the Arduino thread
        std::cerr << "Stopping Arduino thread..." << std::endl;
        g_arduino_running = false;
        if (io_ctx)
        {
            try
            {
                io_ctx->stop();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception stopping io_ctx: " << e.what() << std::endl;
            }
        }

        if (g_arduino_thread && g_arduino_thread->joinable())
        {
            try
            {
                g_arduino_thread->join();
                std::cerr << "Arduino thread joined successfully" << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Exception joining Arduino thread: " << e.what() << std::endl;
            }
        }

        // Properly shut down the server
        if (g_server_running)
        {
            std::cout << "Stopping Crow server..." << std::endl;
            g_crow_app.stop();

            if (g_server_thread && g_server_thread->joinable())
            {
                std::cout << "Waiting for server thread to join..." << std::endl;
                g_server_thread->join();
                std::cout << "Server thread joined." << std::endl;
            }
        }

        // Remove tray icon and close
        RemoveTrayIcon(hWnd);
        PostQuitMessage(0);
        break;

    case WM_USER_TRAYICON:
        HandleTrayIconClick(hWnd, lParam);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }

    return (INT_PTR)FALSE;
}
