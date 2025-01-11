#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "json.hpp" // Include nlohmann::json

using json = nlohmann::json;

struct Configuration {
    std::wstring com_port;
	std::vector<std::string> sliders;
};

// Function to write data to a JSON file
bool writeToJson(const std::vector<std::string> sliders, std::wstring com_port) {
    try {
        json data;
		data["com_port"] = com_port;
		for (int i = 0; i < sliders.size(); i++) {
			data["slider"]["slider" + std::to_string(i + 1)] = sliders[i];
		}

        std::ofstream outfile("config.txt");
        if (outfile.is_open()) {
            outfile << data.dump(4);
            outfile.close();
            std::cout << "Data written to " << "config.txt" << std::endl;
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
	std::wstring com_port;
    std::vector<std::string> sliders;
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
			}
            if (data.contains("slider") &&
                data["slider"].contains("slider1") &&
                data["slider"].contains("slider2") &&
                data["slider"].contains("slider3") &&
                data["slider"].contains("slider4")) 
			{
                config.sliders.push_back(data["slider"]["slider1"].get<std::string>());
                config.sliders.push_back(data["slider"]["slider2"].get<std::string>());
                config.sliders.push_back(data["slider"]["slider3"].get<std::string>());
                config.sliders.push_back(data["slider"]["slider4"].get<std::string>());
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