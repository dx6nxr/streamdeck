#pragma once

#include <string>

// Forward declaration of json (if you don't want to include json.hpp in the header)
namespace nlohmann {
    class json;
}

// Function prototypes
bool writeToJson(const std::vector<std::string> sliders);
std::vector<std::string> readFromJson();