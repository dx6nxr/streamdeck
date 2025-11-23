#ifndef BACKEND_LOGIC_HPP
#define BACKEND_LOGIC_HPP

#include <string>
#include <mutex>
#include <optional> // Include <optional> to resolve E0135
#include "json.hpp" // Or path/to/json.hpp - Needed for json type alias
#include "crow.h"   // Or path/to/crow.h - Needed for crow::response

// Use nlohmann/json namespace alias in the header for convenience
using json = nlohmann::json;

// --- Configuration Constants ---
extern const std::string CONFIG_FILE;
extern const std::string BINDS_FILE;
extern const int SERVER_PORT;

// --- Global Mutexes (Declared here, defined in .cpp) ---
extern std::mutex config_mutex;
extern std::mutex binds_mutex;

// --- Function Declarations (Prototypes) ---

/**
 * @brief Reads the content of a text file.
 * @param filepath Path to the file.
 * @return std::optional<std::string> containing file content if successful, std::nullopt otherwise.
 */
std::optional<std::string> readFileContent(const std::string &filepath);

/**
 * @brief Determines the MIME type based on file extension.
 * @param filepath Path to the file.
 * @return C-string representing the MIME type (e.g., "text/html"). Defaults to "application/octet-stream".
 */
const char *getMimeType(const std::string &filepath);

/**
 * @brief Reads JSON data from a specified file.
 * @param filename The path to the JSON file.
 * @param file_mutex A mutex to lock during file access.
 * @return nlohmann::json object containing parsed data, or a default empty object/array on error/not found.
 */
json readJsonFile(const std::string &filename, std::mutex &file_mutex);

/**
 * @brief Writes JSON data to a specified file.
 * @param filename The path to the JSON file.
 * @param data The nlohmann::json object to write.
 * @param file_mutex A mutex to lock during file access.
 * @return True if writing was successful, false otherwise.
 */
bool writeJsonFile(const std::string &filename, const json &data, std::mutex &file_mutex);

/**
 * @brief Adds standard CORS headers to a Crow response object.
 * @param res The crow::response object to modify.
 */
void addCorsHeaders(crow::response &res);

/**
 * @brief Converts a key name string to Windows virtual key code.
 * @param keyName String representation of the key.
 * @return Windows virtual key code (VK_*) or 0 if unknown.
 */
int getVirtualKeyCode(const std::string &keyName);

/**
 * @brief Simulates pressing and releasing a key combination.
 * @param keyCode Windows virtual key code of the main key.
 * @param modifiers Bit flags for modifiers (1=Ctrl, 2=Shift, 4=Alt).
 */
void simulateHotkey(int keyCode, int modifiers);

/**
 * @brief Simulates pressing and releasing a multimedia key.
 * @param mediaKey Windows virtual key code for the media key.
 * @return True if successful, false otherwise.
 */
bool simulateMediaKey(int mediaKey);

#endif // BACKEND_LOGIC_HPP#include "backend_logic.hpp"