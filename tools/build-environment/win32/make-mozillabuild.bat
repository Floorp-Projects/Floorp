rem This script is specific to the paths on bsmedberg's machine. Alter to
rem suit your environment.

set VC8DIR=C:\Program Files\Microsoft Visual Studio 8
set SDKDIR=C:\Program Files\Microsoft Platform SDK 2003SP1
set PYTHONDIR=C:\python25
set SRCDIR=%~dp0%

call "%VC8DIR%\VC\bin\vcvars32.bat"
set INCLUDE=%SDKDIR%\Include\atl;%INCLUDE%

rmdir /S %SRCDIR%\_obj
mkdir %SRCDIR%\_obj

cd %SRCDIR%\_obj
%PYTHONDIR%\python.exe ..\packageit.py

pause
