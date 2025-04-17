@echo off
echo Building and starting the StreamDeck WASAPI application...

REM Build the UI first
echo Building UI...
call npm run build

REM Start the UI in development mode in a new window
echo Starting UI in development mode...
start cmd /k "npm run dev"

REM Go back to the parent directory and start the C++ backend
echo Starting C++ backend...
cd ..
start streamdeck-wasapi.exe

echo Application started successfully!
echo UI is available at http://localhost:3000
echo C++ Backend API is available at http://localhost:8080/api
echo Press any key to exit this window...
pause > nul 