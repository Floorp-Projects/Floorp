@echo OFF
REM --------------------------------------------------------
REM Modify GRE_DIR below to match your GRE install location
REM --------------------------------------------------------
set GRE_VERSION=1.3b

if     exist "%ProgramFiles%\nul" set GRE_DIR=%ProgramFiles%\common files\mozilla.org\GRE\%GRE_VERSION%
if not exist "%ProgramFiles%\nul" set GRE_DIR=c:\Program Files\common files\mozilla.org\GRE\%GRE_VERSION%

if not exist "%GRE_DIR%\nul" echo.
if not exist "%GRE_DIR%\nul" echo. GRE version %GRE_VERSION% not found at: %GRE_DIR%
if not exist "%GRE_DIR%\nul" echo.
if not exist "%GRE_DIR%\nul" goto end

set PATH=%GRE_DIR%;%GRE_DIR%\components;%PATH%
mfcembed.exe -console
goto end

:end
