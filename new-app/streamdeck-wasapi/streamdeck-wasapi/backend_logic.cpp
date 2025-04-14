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
const int SERVER_PORT = 8080; // Make sure this matches API_URLS in script.js

// --- Define Global Mutexes ---
// Define the actual mutex objects declared as extern in the header
std::mutex config_mutex;
std::mutex binds_mutex;


// --- Function Definitions ---

json readJsonFile(const std::string& filename, std::mutex& file_mutex) {
    json data;
    
    try {
        std::lock_guard<std::mutex> lock(file_mutex);
        
        std::ifstream input_file(filename);
        if (!input_file.is_open()) {
            std::cerr << "Warning: Could not open file " << filename << std::endl;
            return filename == BINDS_FILE ? json::array() : json::object();
        }
        
        // Check if file is empty
        input_file.seekg(0, std::ios::end);
        if (input_file.tellg() == 0) {
            std::cerr << "Warning: File " << filename << " is empty" << std::endl;
            // Return empty array for binds.json, empty object for config.json
            return filename == BINDS_FILE ? json::array() : json::object();
        }
        
        // Reset file position to beginning
        input_file.seekg(0, std::ios::beg);
        
        try {
            input_file >> data;
        }
        catch (json::parse_error& e) {
            std::cerr << "Error: Failed to parse JSON file " << filename << ". Error: " << e.what() << std::endl;
            // Return empty array for binds.json, empty object for config.json
            return filename == BINDS_FILE ? json::array() : json::object();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in readJsonFile: " << e.what() << std::endl;
        // Return empty array for binds.json, empty object for config.json
        return filename == BINDS_FILE ? json::array() : json::object();
    }
    
    // Validate the data structure: binds.json should be an array, config.json should be an object
    if (filename == BINDS_FILE && !data.is_array()) {
        std::cerr << "Warning: " << filename << " does not contain a valid array, resetting" << std::endl;
        return json::array();
    }
    else if (filename == CONFIG_FILE && !data.is_object()) {
        std::cerr << "Warning: " << filename << " does not contain a valid object, resetting" << std::endl;
        return json::object();
    }
    
    return data;
}

bool writeJsonFile(const std::string& filename, const json& data, std::mutex& file_mutex) {
    std::lock_guard<std::mutex> lock(file_mutex);
    std::ofstream file_stream(filename);
    if (!file_stream.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return false;
    }
    try {
        file_stream << data.dump(4); // Pretty-print
        file_stream.close();
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: Failed to write JSON to file " << filename << ". Error: " << e.what() << std::endl;
        file_stream.close();
        return false;
    }
}

void addCorsHeaders(crow::response& res) {
    // Allow requests from any origin (adjust for production safety)
    res.add_header("Access-Control-Allow-Origin", "*");
    // Specify allowed methods for actual requests
    res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    // Specify allowed headers in actual requests (Content-Type is common)
    res.add_header("Access-Control-Allow-Headers", "Content-Type");
}

std::optional<std::string> readFileContent(const std::string& filepath) {
    std::ifstream file_stream(filepath);
    if (!file_stream.is_open()) {
        std::cerr << "Warning: Could not open static file: " << filepath << std::endl;
        return std::nullopt; // Indicate failure
    }
    std::stringstream buffer;
    buffer << file_stream.rdbuf(); // Read entire file into the buffer
    file_stream.close();
    return buffer.str(); // Return content as string
}

const char* getMimeType(const std::string& filepath) {
    size_t dot_pos = filepath.rfind('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream"; // Default binary type
    }

    std::string ext = filepath.substr(dot_pos + 1);

    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    // Add more MIME types as needed

    return "application/octet-stream"; // Default
}

