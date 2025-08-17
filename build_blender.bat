@echo off
setlocal enabledelayedexpansion

set "ROOT_DIR=%~dp0"
set "BUILD_DIR=%ROOT_DIR%build"

for %%i in (git cmake) do (
  where %%i >NUL 2>&1 || (
    echo Error: required command '%%i' not found
    exit /b 1
  )
)

rem Update submodules if repository
if exist "%ROOT_DIR%\.git" (
  git submodule update --init --recursive
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release %CMAKE_ARGS%
cmake --build . --config Release --target INSTALL

endlocal
