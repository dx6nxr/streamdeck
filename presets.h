#pragma once

#include <string>
#include <vector>
#include "backend.h"

struct Configuration {
    std::wstring com_port;
    int num_sliders;
    int num_btns;
    std::vector<std::string> sliders;
    std::vector<MultimediaButton> bindings;
    bool isValid = false;
};

// Function prototypes
bool writeToJson(const std::vector <AudioDevice>& sliders,
                 const std::wstring& com_port,
                 const std::vector<MultimediaButton>& bindings,
                 const std::string& filename = "config.json");
Configuration readFromJson(const std::string& filename = "config.json");

bool saveKeyBinds(vector<MultimediaButton> buttons, const std::string& filename = "binds.json");
vector<MultimediaButton> loadKeyBinds(const std::string& filename = "binds.json");
