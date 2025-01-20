#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "json.hpp" // Include nlohmann::json

using json = nlohmann::json;

struct Configuration {
    std::wstring com_port;
	std::vector<std::string> sliders;
    std::vector<unsigned short int> buttons;
    bool isValid = false;
};

// Function to write data to a JSON file
bool writeToJson(const std::vector<std::string> sliders, std::wstring com_port, std::vector<unsigned short int> keyMaps) {
    try {
        json data;
		data["com_port"] = com_port;
		for (int i = 0; i < sliders.size(); i++) {
			data["sliders"]["slider" + std::to_string(i + 1)] = sliders[i];
		}
        for (int i = 0; i < keyMaps.size(); i++) {
            data["buttons"]["button" + std::to_string(i + 1)] = keyMaps[i];
        }

        std::ofstream outfile("config.txt");
        if (outfile.is_open()) {
            outfile << data.dump(4);
            outfile.close();
            return true;
        }
        else {
            std::cerr << "Error opening file for writing: " << "config.txt" << std::endl;
            return false;
        }
    }
    catch (json::exception& e) {
        std::cerr << "JSON exception: " << e.what() << std::endl;
        return false;
    }
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
            if (data.contains("sliders"))
			{
                config.sliders.push_back(data["sliders"]["slider1"].get<std::string>());
                config.sliders.push_back(data["sliders"]["slider2"].get<std::string>());
                config.sliders.push_back(data["sliders"]["slider3"].get<std::string>());
                config.sliders.push_back(data["sliders"]["slider4"].get<std::string>());
            }
            else {
                std::cerr << "Invalid JSON structure in " << "config.txt" << std::endl;
                return config;
            }
            if (data.contains("buttons"))
            {
                config.buttons.push_back(data["buttons"]["button1"].get<unsigned short int>());
                config.buttons.push_back(data["buttons"]["button2"].get<unsigned short int>());
                config.buttons.push_back(data["buttons"]["button3"].get<unsigned short int>());
                config.buttons.push_back(data["buttons"]["button4"].get<unsigned short int>());
                config.buttons.push_back(data["buttons"]["button5"].get<unsigned short int>());
                config.buttons.push_back(data["buttons"]["button6"].get<unsigned short int>());
                config.buttons.push_back(data["buttons"]["button7"].get<unsigned short int>());
                config.buttons.push_back(data["buttons"]["button8"].get<unsigned short int>());
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