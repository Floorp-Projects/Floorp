@echo OFF
REM --------------------------------------------------------
REM Modify GRE_DIR below to match your GRE install location
REM --------------------------------------------------------
set GRE_VERSION=1.2b
set GRE_DIR=c:\Program Files\Mozilla\GRE\%GRE_VERSION%

set PATH=%PATH%;%GRE_DIR%;%GRE_DIR%\components
mfcembed.exe -console

