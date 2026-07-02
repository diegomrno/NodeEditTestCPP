@echo off
setlocal EnableExtensions EnableDelayedExpansion

if not exist "..\lib\vortex\tests\project\.vx\modules" (
    mkdir "..\lib\vortex\tests\project\.vx\modules"
)

xcopy /E /I /Y "..\dist\*" "..\lib\vortex\tests\project\.vx\modules"

set /p VERSION=<"..\lib\vortex\version.conf"
set "SCRIPT_DIR=%~dp0"
set "VORTEX_PATH=%SCRIPT_DIR%..\lib\vortex\build\dist\%VERSION%\bin"
set "PROJECT_PATH=%SCRIPT_DIR%..\lib\vortex\tests\project"

call "%VORTEX_PATH%\vx.bat" "%PROJECT_PATH%"