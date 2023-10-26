REM Copyright 2022 The Chromium Authors.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.
@echo off
setlocal

REM This script is meant to be run once to setup the example demo agent.
REM Run it with one command line argument: the path to a directory where the
REM demo agent will be built. This should be a directory outside the SDK
REM directory tree. By default, if no directory is supplied, a directory
REM named `build` in the project root will be used.
REM
REM Once the build is prepared, the demo binary is built using the command
REM `cmake --build <build-dir>`, where <build-dir> is the same argument given
REM to this script.

set ROOT_DIR=%~dp0
call :ABSPATH "%ROOT_DIR%\demo" DEMO_DIR
call :ABSPATH "%ROOT_DIR%\proto" PROTO_DIR

REM BUILD_DIR defaults to $ROOT_DIR/build if no argument is provided.
IF "%1" == "" (
  call :ABSPATH "%ROOT_DIR%\build" BUILD_DIR
) ELSE (
  set BUILD_DIR=%~f1
)

echo .
echo Root dir:   %ROOT_DIR%
echo Build dir:  %BUILD_DIR%
echo Demo dir:   %DEMO_DIR%
echo Proto dir:  %PROTO_DIR%
echo .

REM Prepare build directory
mkdir "%BUILD_DIR%"
REM Prepare protobuf out directory
mkdir "%BUILD_DIR%\gen"
REM Enter build directory
cd /d "%BUILD_DIR%"

REM Install vcpkg and use it to install Google Protocol Buffers.
IF NOT exist .\vcpkg\ (
  cmd/c git clone https://github.com/microsoft/vcpkg
  cmd/c .\vcpkg\bootstrap-vcpkg.bat -disableMetrics
) ELSE (
  echo vcpkg is already installed.
)
REM Install any packages we want from vcpkg.
cmd/c .\vcpkg\vcpkg install protobuf:x64-windows
cmd/c .\vcpkg\vcpkg install gtest:x64-windows 

REM Generate the build files.
set CMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake %ROOT_DIR%

echo.
echo.
echo To build, type: cmake --build "%BUILD_DIR%"
echo.

exit /b

REM Resolve relative path in %1 and set it into variable %2.
:ABSPATH
  set %2=%~f1
  exit /b

