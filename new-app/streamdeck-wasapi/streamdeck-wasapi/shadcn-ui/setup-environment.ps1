# PowerShell script to set environment variables for StreamDeck WASAPI
Write-Host "Setting up environment variables for StreamDeck WASAPI with shadcn UI..."

# Get the current directory and parent directory
$currentDir = Get-Location
$parentDir = Split-Path -Parent $currentDir

# Set environment variables for the application
[Environment]::SetEnvironmentVariable("SHADCN_UI_PATH", "$currentDir\.next\static", [System.EnvironmentVariableTarget]::Process)
[Environment]::SetEnvironmentVariable("STREAMDECK_APP_PATH", "$parentDir", [System.EnvironmentVariableTarget]::Process)

# Display set variables
Write-Host "SHADCN_UI_PATH = $([Environment]::GetEnvironmentVariable('SHADCN_UI_PATH', [System.EnvironmentVariableTarget]::Process))"
Write-Host "STREAMDECK_APP_PATH = $([Environment]::GetEnvironmentVariable('STREAMDECK_APP_PATH', [System.EnvironmentVariableTarget]::Process))"

Write-Host "Environment setup complete." 