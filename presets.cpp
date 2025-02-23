#include <iostream>
#include <fstream>
#include <qobject.h>
#include <string>
#include <vector>
#include "json.hpp" // Include nlohmann::json
#include "backend.h"
#include "presets.h"

using json = nlohmann::json;

// Function to write data to a JSON file
bool saveKeyBinds(vector<MultimediaButton> buttons, const std::string& filename){
    json data;
    for (const auto& button: buttons) {
        json buttonJson;
        buttonJson["name"] = button.name.toStdString();
        buttonJson["keyCodes"] = button.keyCodes;
        data.push_back(buttonJson);
    }

    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << std::setw(4) << data << std::endl;
        outfile.close();
        return true;
    } else {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return false;
    }
}

vector<MultimediaButton> loadKeyBinds(const std::string& filename){
    vector<MultimediaButton> buttons;
    std::ifstream infile(filename);
    if (infile.is_open()) {
        try {
            json data;
            infile >> data;
            for (const auto& bindingJson: data) {
                QString name = QString::fromStdString(bindingJson["name"].get<std::string>());
                std::vector<int> keyCodes = bindingJson["keyCodes"].get<std::vector<int>>();
                buttons.emplace_back(MultimediaButton(name, keyCodes));
            }
            return buttons;
        } catch (json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << " in " << filename << std::endl;
        }
        infile.close();
    } else {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
    }
    return buttons;
}

bool writeToJson(const std::vector <AudioDevice>& sliders,
                 const std::wstring& com_port,
                 const std::vector<MultimediaButton>& bindings, // Neuer Parameter für die Bindungen
                 const std::string& filename) {

    json data;
    data["com_port"] = com_port;
    json slidersJson = json::array();
    for (const auto& slider: sliders) {
        slidersJson.push_back(slider.name.toStdString());
    }
    data["sliders"] = slidersJson;

    // Speichern der neuen Bindungen
    json bindingsJson = json::array();
    for (const auto& binding: bindings) {
        json bindingJson;
        bindingJson["name"] = binding.name.toStdString();
        bindingJson["keyCodes"] = binding.keyCodes; // Direkte Zuweisung des Vektors
        bindingsJson.push_back(bindingJson);
    }
    data["bindings"] = bindingsJson; // Speichern unter "bindings"

    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << std::setw(4) << data << std::endl;
        outfile.close();
        return true;
    } else {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return false;
    }
}

// Function to read data from a JSON file
Configuration readFromJson(const std::string& filename) {
    Configuration config;
    std::ifstream infile(filename);

    if (infile.is_open()) {
        try {
            json data;
            infile >> data;

            if (data.contains("com_port") && data.contains("sliders") && data.contains("bindings")) {
                config.com_port = data["com_port"].get<std::wstring>();
                config.sliders = data["sliders"].get<std::vector<std::string>>();
                for (const auto& bindingJson: data["bindings"]) {
                    QString name = QString::fromStdString(bindingJson["name"].get<std::string>());
                    std::vector<int> keyCodes = bindingJson["keyCodes"].get<std::vector<int>>();
                    config.bindings.emplace_back(name, keyCodes); // Speichern in config.bindings
                }
                config.isValid = true;
                return config;
            } else {
                std::cerr << "Invalid JSON structure in " << filename << std::endl;
            }
        } catch (json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << " in " << filename << std::endl;
        }
        infile.close();
    } else {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
    }
    return config;
}
