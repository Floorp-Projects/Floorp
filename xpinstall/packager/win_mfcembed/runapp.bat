@echo OFF
REM --------------------------------------------------------
REM Modify MRE_DIR below to match your MRE install location
REM --------------------------------------------------------
set MRE_DIR=c:\Program Files\Mozilla\MRE\1.0
set PATH=%PATH%;%MRE_DIR%;%MRE_DIR%\components
mfcembed.exe -console

