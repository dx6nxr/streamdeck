#pragma once

#include <string>
#include <vector>

struct Configuration {
    std::wstring com_port;
    int num_sliders;
    int num_btns;
    std::vector<std::string> sliders;
    std::vector<unsigned short int> buttons;
    bool isValid = false;
};

// Forward declaration of json (if you don't want to include json.hpp in the header)
namespace nlohmann {
class json;
}

// Function prototypes
bool writeToJson(const std::vector<std::string> sliders, std::wstring com_port, std::vector<unsigned short int> keyMaps);
Configuration readFromJson();
