# Integration Guide: Shadcn UI with C++ Backend

This guide explains how to integrate the new shadcn UI with the existing C++ backend (streamdeck-wasapi.cpp).

## Architecture Overview

The integration uses two separate components:

1. **C++ Backend (streamdeck-wasapi.exe)**: 
   - Handles WASAPI audio functionality
   - Controls Arduino communication
   - Provides REST API endpoints to the UI
   - Runs on port 8080

2. **Shadcn UI (Next.js)**:
   - Provides a modern user interface
   - Communicates with the backend via API calls
   - Runs on port 3000 during development

## Setup Instructions

### Option 1: Integrated Startup (Recommended)

1. Navigate to the `shadcn-ui` directory
2. Run the batch script:
   ```
   .\start-app.bat
   ```
   This will:
   - Build the UI (`npm run build`)
   - Start the UI in development mode (`npm run dev`)
   - Start the C++ backend in a separate window

### Option 2: Manual Setup

If you prefer to start components individually:

1. **Build the UI**:
   ```
   cd shadcn-ui
   npm run build
   ```

2. **Start the UI in development mode**:
   ```
   npm run dev
   ```

3. **Start the C++ backend**:
   ```
   cd ..
   .\streamdeck-wasapi.exe
   ```

## Troubleshooting

### API Connection Issues

If the UI cannot connect to the backend API:

1. Ensure the backend is running and showing the message "Server running on http://localhost:8080"
2. Check that the port numbers match in both applications
3. Check browser console for CORS errors - the backend should allow cross-origin requests

### UI Display Issues

If UI components don't load correctly:

1. Clear browser cache and reload
2. Ensure all npm dependencies are installed: `npm install`
3. Restart both the UI and backend processes

## Development Notes

### Modifying API Endpoints

If you need to add new API endpoints:

1. Add the endpoint in the C++ backend in `streamdeck-wasapi.cpp`:
   ```cpp
   CROW_ROUTE(g_crow_app, "/api/your-endpoint").methods("GET"_method)
   ([](const crow::request& req, crow::response& res) {
       // Your implementation
       addCorsHeaders(res);
       res.write(json_response.dump());
       res.end();
   });
   ```

2. Add the corresponding URL in the UI's `API_URLS` object in `src/app/page.tsx`

### Building for Production

To deploy the integrated application:

1. Build the UI for production:
   ```
   cd shadcn-ui
   npm run build
   ```

2. Update the C++ backend to serve the production build directly instead of redirecting to the development server

## C++ Backend Modifications

The following changes were made to the C++ backend to support the shadcn UI:

1. Added static file serving for UI assets
2. Updated API response formats to match shadcn UI expectations
3. Added environment variables for configuration
4. Enhanced CORS support for cross-origin requests 