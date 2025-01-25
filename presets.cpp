#include <iostream>
#include <fstream>
#include <qobject.h>
#include <string>
#include <vector>
#include "json.hpp" // Include nlohmann::json

using json = nlohmann::json;

struct Configuration {
    std::wstring com_port;
    int num_sliders;
    int num_btns;
    std::vector<std::string> sliders;
    std::vector<unsigned short int> buttons;
    bool isValid = false;
};

// Function to write data to a JSON file
bool writeToJson(const std::vector<std::string> sliders, std::wstring com_port, std::vector<unsigned short int> keyMaps) {
    json data;
    data["com_port"] = com_port;
    data["num_sliders"] = sliders.size();
    data["num_buttons"] = keyMaps.size();
    json slidersJson;
    for (int i = 0; i < sliders.size(); ++i) {
        slidersJson["slider" + std::to_string(i + 1)] = sliders[i];
    }
    data["sliders"] = slidersJson;
    json buttonsJson;
    for (int i = 0; i < keyMaps.size(); ++i) {
        buttonsJson["button" + std::to_string(i + 1)] = keyMaps[i];
    }
    data["buttons"] = buttonsJson;
    //write to condig.txt
    std::ofstream outfile("config.txt");
    if (outfile.is_open()) {
        outfile << data.dump(4);
        outfile.close();
    }
    else {
        std::cerr << "Error opening file for writing: " << "config.txt" << std::endl;
        return false;
    }
    return true;
}

// Function to read data from a JSON file
Configuration readFromJson() {
    Configuration config;
    try {
        std::ifstream infile("config.txt");
        if (infile.is_open()) {
            json data;
            infile >> data;
            infile.close();
            if (data.contains("com_port")) {
                config.com_port = data["com_port"].get<std::wstring>();
            }
            else {
                std::cerr << "Invalid JSON structure in " << "config.txt" << std::endl;
                return config;
            }
            if (data.contains("num_sliders")) {
                config.num_sliders = data["num_sliders"].get<int>();
            }
            else {
                std::cerr << "Invalid JSON structure in " << "config.txt" << std::endl;
                return config;
            }
            if (data.contains("num_buttons")) {
                config.num_btns = data["num_buttons"].get<int>();
            }
            else {
                std::cerr << "Invalid JSON structure in " << "config.txt" << std::endl;
                return config;
            }
            if (data.contains("sliders"))
            {
                for (int i = 0; i < config.num_sliders; ++i) {
                    config.sliders.push_back(data["sliders"]["slider" + std::to_string(i + 1)].get<std::string>());
                }
            }
            else {
                std::cerr << "Invalid JSON structure in " << "config.txt" << std::endl;
                return config;
            }
            if (data.contains("buttons"))
            {
                for (int i = 0; i < config.num_btns; ++i) {
                    config.buttons.push_back(data["buttons"]["button" + std::to_string(i + 1)].get<unsigned short int>());
                }
                config.isValid = true;
                return config;
            }
            else {
                std::cerr << "Invalid JSON structure in " << "config.txt" << std::endl;
                return config;
            }
        }
        else {
            std::cerr << "Error opening file for reading: " << "config.txt" << std::endl;
            return config;
        }
    }
    catch (json::exception& e) {
        std::cerr << "JSON exception: " << e.what() << std::endl;
        return config;
    }
}
