@echo off
setlocal enabledelayedexpansion

rd /s /q ..\dist 2>nul
rd /s /q ..\build 2>nul
rd /s /q ..\lib\vortex\tests\project\.vx\modules\ 2>nul
mkdir ..\build
cd ..\build

cmake -G "Visual Studio 17 2022" -A x64 ..

for /f %%i in ('powershell -command "(Get-WmiObject -Class Win32_Processor).NumberOfLogicalProcessors"') do set THREADS=%%i
cmake --build . --config Release -- /m:%THREADS%
cmake --install . --config Release

cd ..\scripts

set MODULE_JSON_PATH=..\module.json
for /f "delims=" %%i in ('powershell -command "(Get-Content '%MODULE_JSON_PATH%' | ConvertFrom-Json).name"') do set NAME=%%i
for /f "delims=" %%i in ('powershell -command "(Get-Content '%MODULE_JSON_PATH%' | ConvertFrom-Json).version"') do set VERSION=%%i
set FOLDER_NAME=%NAME%-%VERSION%-win

mkdir "..\dist\%FOLDER_NAME%"

xcopy /e /i /y "..\build\Release\*" "..\dist\%FOLDER_NAME%\" 2>nul
xcopy /e /i /y "..\main\assets\*" "..\dist\%FOLDER_NAME%\" 2>nul
copy "..\module.json" "..\dist\%FOLDER_NAME%\" 2>nul
copy "..\LICENSE" "..\dist\%FOLDER_NAME%\" 2>nul
copy "..\CREDITS" "..\dist\%FOLDER_NAME%\" 2>nul
copy "..\README.md" "..\dist\%FOLDER_NAME%\" 2>nul

rd /s /q "..\dist\%FOLDER_NAME%\CMakeFiles" 2>nul
rd /s /q "..\dist\%FOLDER_NAME%\scripts" 2>nul
rd /s /q "..\dist\%FOLDER_NAME%\.git" 2>nul
rd /s /q "..\dist\%FOLDER_NAME%\.vscode" 2>nul
del "..\dist\%FOLDER_NAME%\remove_resources_if_exists.cmake" 2>nul
del "..\dist\%FOLDER_NAME%\cmake_install.cmake" 2>nul
del "..\dist\%FOLDER_NAME%\CMakeCache.txt" 2>nul
del "..\dist\%FOLDER_NAME%\dist.tar.gz" 2>nul
del "..\dist\%FOLDER_NAME%\Makefile" 2>nul
del "..\dist\%FOLDER_NAME%\.gitmodules" 2>nul
del "..\dist\%FOLDER_NAME%\CMakeLists.txt" 2>nul

set TAR_NAME=..\build\%FOLDER_NAME%.tar.gz
powershell -command "tar -czf '%TAR_NAME%' -C '..\dist' '%FOLDER_NAME%'"
echo Archive created : %TAR_NAME%

endlocal