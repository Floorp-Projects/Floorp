@echo off

SET MOZ_MSVCVERSION=7.1

SET MOZILLABUILDDRIVE=%~d0%
SET MOZILLABUILDPATH=%~p0%
SET MOZILLABUILD=%MOZILLABUILDDRIVE%%MOZILLABUILDPATH%

echo "Mozilla tools directory: %MOZILLABUILD%"

REM Get MSVC paths
call "%MOZILLABUILD%\guess-msvc.bat"

if "%VC71DIR%"=="Not Found" (
    ECHO "Microsoft Visual C++ version 7.1 was not found. Exiting."
    pause
    EXIT /B 1
)

REM Use the "new" moztools-static
set MOZ_TOOLS=%MOZILLABUILD%\moztools

SET INCLUDE=
SET LIB=

rem append moztools to PATH
SET PATH=%PATH%;%MOZ_TOOLS%\bin

rem Prepend MSVC paths
call "%VC71DIR%\Bin\vcvars32.bat"

cd "%USERPROFILE%"
start "MSYS Shell - MSVC7.1 Environment" "%MOZILLABUILD%\msys\bin\rxvt" -backspacekey  -sl 2500 -fg %FGCOLOR% -bg %BGCOLOR% -sr -fn "Lucida Console" -tn msys -geometry 80x25 -e /bin/sh --login -i
