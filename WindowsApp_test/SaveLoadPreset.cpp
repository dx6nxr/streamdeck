#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "json.hpp" // Include nlohmann::json

using json = nlohmann::json;

// Function to write data to a JSON file
bool writeToJson(const std::vector<std::string> sliders) {
    try {
        json data;
		for (int i = 0; i < sliders.size(); i++) {
			data["slider"]["slider" + std::to_string(i + 1)] = sliders[i];
		}

        std::ofstream outfile("config.txt");
        if (outfile.is_open()) {
            outfile << data.dump(4); // Use dump(4) for pretty printing
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
std::vector<std::string> readFromJson() {
    std::vector<std::string> sliders;
    try {
        std::ifstream infile("config.txt");
        if (infile.is_open()) {
            json data;
            infile >> data;
            infile.close();

            if (data.contains("slider") &&
                data["slider"].contains("slider1") &&
                data["slider"].contains("slider2") &&
                data["slider"].contains("slider3") &&
                data["slider"].contains("slider4")) 
			{
                sliders.push_back(data["slider"]["slider1"].get<std::string>());
                sliders.push_back(data["slider"]["slider2"].get<std::string>());
                sliders.push_back(data["slider"]["slider3"].get<std::string>());
                sliders.push_back(data["slider"]["slider4"].get<std::string>());
				return sliders;
            }
            else {
                std::cerr << "Invalid JSON structure in " << "config.txt" << std::endl;
				return sliders;
            }
        }
        else {
            std::cerr << "Error opening file for reading: " << "config.txt" << std::endl;
            return sliders;
        }
    }
    catch (json::exception& e) {
        std::cerr << "JSON exception: " << e.what() << std::endl;
        return sliders;
    }
}