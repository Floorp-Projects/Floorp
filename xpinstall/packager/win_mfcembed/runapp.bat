@echo OFF
REM --------------------------------------------------------
REM Modify GRE_DIR below to match your GRE install location
REM --------------------------------------------------------
set GRE_VERSION=1.3a
set GRE_DIR=c:\Program Files\mozilla.org\GRE\%GRE_VERSION%

set PATH=%PATH%;%GRE_DIR%;%GRE_DIR%\components
mfcembed.exe -console

