#pragma once

#include <string>

struct Configuration {
    std::wstring com_port;
    std::vector<std::string> sliders;
};

// Forward declaration of json (if you don't want to include json.hpp in the header)
namespace nlohmann {
    class json;
}

// Function prototypes
bool writeToJson(const std::vector<std::string> sliders, std::wstring com_port);
Configuration readFromJson();