@echo off

SET MOZ_MSVCVERSION=6

SET MOZILLABUILDDRIVE=%~d0%
SET MOZILLABUILDPATH=%~p0%
SET MOZILLABUILD=%MOZILLABUILDDRIVE%%MOZILLABUILDPATH%

echo "Mozilla tools directory: %MOZILLABUILD%"

REM Get MSVC paths
call "%MOZILLABUILD%\guess-msvc.bat"

if "%VC6DIR%"=="" (
    ECHO "Microsoft Visual C++ version 6 was not found. Exiting."
    pause
    EXIT /B 1
)

REM For MSVC6, we use the "old" non-static moztools
set MOZ_TOOLS=%MOZILLABUILD%\moztools-180compat

rem append moztools to PATH
SET PATH=%PATH%;%MOZ_TOOLS%\bin

rem Prepend MSVC paths
call "%VC6DIR%\Bin\vcvars32.bat"

cd "%USERPROFILE%"
start "MSYS Shell - MSVC6 Environment" "%MOZILLABUILD%\msys\bin\rxvt" -backspacekey  -sl 2500 -fg %FGCOLOR% -bg %BGCOLOR% -sr -fn "Lucida Console" -tn msys -geometry 80x25 -e /bin/bash --login -i
