@echo off

SET MOZ_MSVCVERSION=7.1

SET MOZILLABUILDDRIVE=%~d0%
SET MOZILLABUILDPATH=%~p0%
SET MOZILLABUILD=%MOZILLABUILDDRIVE%%MOZILLABUILDPATH%

echo "Mozilla tools directory: %MOZILLABUILD%"

REM Get MSVC paths
call "%MOZILLABUILD%\guess-msvc.bat"

if "%VC71DIR%"=="" (
    ECHO "Microsoft Visual C++ version 7.1 was not found. Exiting."
    pause
    EXIT /B 1
)

REM Use the "new" moztools-static
set MOZ_TOOLS=%MOZILLABUILD%\moztools

rem append moztools to PATH
SET PATH=%PATH%;%MOZ_TOOLS%\bin

rem Prepend MSVC paths
call "%VC71DIR%\Bin\vcvars32.bat"

rem Prepend SDK paths if MOZBUILD_USE_SDK is defined and the SDK exists
if defined MOZBUILD_USE_SDK (
    if not "%SDKDIR%"=="" (
        rem Prepend SDK paths - Don't use the SDK SetEnv.cmd because it pulls in
        rem random VC paths which we don't want.

        rem Don't add '%SDKDIR%include\atl' to INCLUDE because VC7 has ATL headers
        rem which don't require atlthunk.lib. This also makes it unnecessary to
        rem append the atlthunk compat lib to LIB.

        SET PATH=%SDKDIR%\bin;%PATH%
        SET LIB=%SDKDIR%\lib;%LIB%
        SET INCLUDE=%SDKDIR%\include;%INCLUDE%
    )
)

cd "%USERPROFILE%"
start "MSYS Shell - MSVC7.1 Environment" "%MOZILLABUILD%\msys\bin\rxvt" -backspacekey  -sl 2500 -fg %FGCOLOR% -bg %BGCOLOR% -sr -fn "Lucida Console" -tn msys -geometry 80x25 -e /bin/bash --login -i
