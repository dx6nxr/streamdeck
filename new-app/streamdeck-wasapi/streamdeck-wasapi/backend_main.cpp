//#define CROW_MAIN // Define this ONLY in the file with main()
//#include "crow.h" // Still need Crow's definitions here
//#include "backend_logic.hpp" // Include our new header
//#include <fstream>
//#include <optional>
//#include <string>
//
//#include <iostream> // For std::cout/cerr
//// Other headers like <fstream>, <string>, <mutex>, json.hpp are included via backend_logic.hpp
//
//// --- Main Application ---
//int main() {
//    crow::SimpleApp app;
//
//    const std::string STATIC_DIR = "public";
//
//    // --- Logging Middleware ---
//    // CROW_MIDDLEWARE_LOG_INFO(app);
//
//    // --- Manual CORS Preflight Handling (OPTIONS request) ---
//    // Define OPTIONS handlers using the helper function
//    auto options_handler = [](const crow::request& /*req*/, crow::response& res) {
//        addCorsHeaders(res);
//        res.code = 204; // No Content
//        res.end();
//        };
//
//    CROW_ROUTE(app, "/")([](const crow::request& /*req*/, crow::response& res) {
//        std::string filepath = "public/index.html";
//        auto content = readFileContent(filepath);
//        if (content) {
//            addCorsHeaders(res); // Optional for static files, but consistent
//            res.add_header("Content-Type", getMimeType(filepath));
//            res.write(*content);
//        }
//        else {
//            res.code = 404;
//            res.write("Not Found: index.html");
//        }
//        res.end();
//        });
//
//    // Route for style.css
//    CROW_ROUTE(app, "/style.css")([](const crow::request& /*req*/, crow::response& res) {
//        std::string filepath = "public/style.css";
//        auto content = readFileContent(filepath);
//        if (content) {
//            addCorsHeaders(res);
//            res.add_header("Content-Type", getMimeType(filepath));
//            res.write(*content);
//        }
//        else {
//            res.code = 404;
//            res.write("Not Found: style.css");
//        }
//        res.end();
//        });
//
//    // Route for script.js
//    CROW_ROUTE(app, "/script.js")([](const crow::request& /*req*/, crow::response& res) {
//        std::string filepath = "public/script.js";
//        auto content = readFileContent(filepath);
//        if (content) {
//            addCorsHeaders(res);
//            res.add_header("Content-Type", getMimeType(filepath));
//            res.write(*content);
//        }
//        else {
//            res.code = 404;
//            res.write("Not Found: script.js");
//        }
//        res.end();
//        });
//
//    CROW_ROUTE(app, "/api/save-config").methods("OPTIONS"_method)(options_handler);
//    CROW_ROUTE(app, "/api/save-binds").methods("OPTIONS"_method)(options_handler);
//    CROW_ROUTE(app, "/api/get-apps").methods("OPTIONS"_method)(options_handler);
//    CROW_ROUTE(app, "/api/load-config").methods("OPTIONS"_method)(options_handler);
//    CROW_ROUTE(app, "/api/load-binds").methods("OPTIONS"_method)(options_handler);
//
//
//    // --- API Routes (using helper functions) ---
//
//    // GET /api/load-config
//    CROW_ROUTE(app, "/api/load-config").methods("GET"_method)
//        ([](const crow::request& /*req*/, crow::response& res) {
//        std::cout << "Received GET /api/load-config" << std::endl;
//        json config_data = readJsonFile(CONFIG_FILE, config_mutex); // Use helper
//        addCorsHeaders(res); // Use helper
//        res.add_header("Content-Type", "application/json");
//        res.write(config_data.dump());
//        res.end();
//            });
//
//    // POST /api/save-config
//    CROW_ROUTE(app, "/api/save-config").methods("POST"_method)
//        ([](const crow::request& req, crow::response& res) {
//        std::cout << "Received POST /api/save-config" << std::endl;
//        addCorsHeaders(res);
//        res.add_header("Content-Type", "application/json");
//
//        json config_data;
//        try {
//            config_data = json::parse(req.body);
//        }
//        catch (json::parse_error& e) {
//            std::cerr << "Error parsing save-config request body: " << e.what() << std::endl;
//            res.code = 400;
//            res.write("{\"error\": \"Invalid JSON format in request body\"}");
//            res.end();
//            return;
//        }
//
//        if (writeJsonFile(CONFIG_FILE, config_data, config_mutex)) { // Use helper
//            res.code = 200;
//            res.write("{\"message\": \"Configuration saved successfully\"}");
//        }
//        else {
//            res.code = 500;
//            res.write("{\"error\": \"Failed to write configuration file\"}");
//        }
//        res.end();
//            });
//
//    // GET /api/load-binds
//    CROW_ROUTE(app, "/api/load-binds").methods("GET"_method)
//        ([](const crow::request& /*req*/, crow::response& res) {
//        std::cout << "Received GET /api/load-binds" << std::endl;
//        json binds_data = readJsonFile(BINDS_FILE, binds_mutex); // Use helper
//        if (!binds_data.is_array()) { // Ensure default is array
//            binds_data = json::array();
//        }
//        addCorsHeaders(res); // Use helper
//        res.add_header("Content-Type", "application/json");
//        res.write(binds_data.dump());
//        res.end();
//            });
//
//    // POST /api/save-binds
//    CROW_ROUTE(app, "/api/save-binds").methods("POST"_method)
//        ([](const crow::request& req, crow::response& res) {
//        std::cout << "Received POST /api/save-binds" << std::endl;
//        addCorsHeaders(res);
//        res.add_header("Content-Type", "application/json");
//
//        json binds_data;
//        try {
//            binds_data = json::parse(req.body);
//            if (!binds_data.is_array()) {
//                throw std::runtime_error("Request body is not a JSON array");
//            }
//        }
//        catch (json::parse_error& e) { /* ... error handling ... */ res.code = 400; res.write("{\"error\": \"Invalid JSON format\"}"); res.end(); return; }
//        catch (const std::exception& e) { /* ... error handling ... */ res.code = 400; res.write("{\"error\": \"" + std::string(e.what()) + "\"}"); res.end(); return; }
//
//
//        if (writeJsonFile(BINDS_FILE, binds_data, binds_mutex)) { // Use helper
//            res.code = 200;
//            res.write("{\"message\": \"Bindings saved successfully\"}");
//        }
//        else {
//            res.code = 500;
//            res.write("{\"error\": \"Failed to write bindings file\"}");
//        }
//        res.end();
//            });
//
//    // POST /api/get-apps
//    CROW_ROUTE(app, "/api/get-apps").methods("POST"_method)
//        ([](const crow::request& /*req*/, crow::response& res) {
//        std::cout << "Received POST /api/get-apps" << std::endl;
//        // --- SIMULATED APP LIST --- (Replace with your actual logic)
//        json app_list = json::array({ "System Monitor", "Web Browser", "Text Editor", "Your C++ App", "Terminal", "File Manager", "Calculator", "Mail Client" });
//        // --------------------------
//        addCorsHeaders(res); // Use helper
//        res.add_header("Content-Type", "application/json");
//        res.write(app_list.dump());
//        res.end();
//            });
//
//    // --- Run the Server ---
//    std::cout << "Starting backend server on port " << SERVER_PORT << "..." << std::endl;
//    std::cout << "Serving static files from: ./" << STATIC_DIR << std::endl;
//    std::cout << "Access UI at: http://localhost:" << SERVER_PORT << "/" << std::endl;
//    app.port(SERVER_PORT)
//        // .multithreaded()
//        .run();
//
//    return 0;
//}