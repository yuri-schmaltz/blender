@echo off
setlocal enabledelayedexpansion

set "ROOT_DIR=%~dp0"
set "BUILD_DIR=%ROOT_DIR%build"
set "LOG_FILE=%ROOT_DIR%install.log"
if exist "%LOG_FILE%" del "%LOG_FILE%"

call :main 2>>"%LOG_FILE%"
if errorlevel 1 (
  echo Errors were encountered during installation. See "%LOG_FILE%" for details.
  exit /b 1
)
exit /b 0

:main
rem Ensure essential build tools are available
for %%i in (git cmake python) do (
  call :check_command %%i
)

rem Use Blender's dependency builder when available
if exist "%ROOT_DIR%build_files\build_environment\windows\build_deps.cmd" (
  echo Ensuring all build dependencies are installed
  call "%ROOT_DIR%build_files\build_environment\windows\build_deps.cmd" 2022 x64
)

rem Update submodules if repository
if exist "%ROOT_DIR%\.git" (
  git submodule update --init --recursive
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release %CMAKE_ARGS%
cmake --build . --config Release --target INSTALL -- /m

endlocal
exit /b 0

:check_command
where %1 >NUL 2>&1 && goto :eof
echo Missing required command '%1'
call :install_tool %1
where %1 >NUL 2>&1 || (
  echo Error: required command '%1' not found
  exit /b 1
)
goto :eof

:install_tool
where choco >NUL 2>&1 && (
  echo Installing %1 using choco...
  choco install -y %1
  goto :eof
)
where winget >NUL 2>&1 && (
  echo Installing %1 using winget...
  winget install -e --id %1
)
goto :eof

