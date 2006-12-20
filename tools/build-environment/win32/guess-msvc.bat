@ECHO OFF

set CYGWIN=
if not defined MOZ_NO_RESET_PATH (
    set PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem
)

SET INCLUDE=
SET LIB=

SET MSVCROOTKEY=HKLM\SOFTWARE\Microsoft\VisualStudio
SET MSVC6KEY=%MSVCROOTKEY%\6.0\Setup\Microsoft Visual C++
SET MSVC71KEY=%MSVCROOTKEY%\7.1\Setup\VC
SET MSVC8KEY=%MSVCROOTKEY%\8.0\Setup\VC

SET VC6DIR=Not Found
SET VC71DIR=Not Found
SET VC8DIR=Not Found

REM First see if we can find MSVC, then set the variable
REG QUERY "%MSVC6KEY%" /v ProductDir > nul
IF %ERRORLEVEL% EQU 0 FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "%MSVC6KEY%" /v ProductDir') DO SET VC6DIR=%%B

REG QUERY "%MSVC71KEY%" /v ProductDir > nul
IF %ERRORLEVEL% EQU 0 FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "%MSVC71KEY%" /v ProductDir') DO SET VC71DIR=%%B

REG QUERY "%MSVC8KEY%" /v ProductDir > nul
IF %ERRORLEVEL% EQU 0 FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "%MSVC8KEY%" /v ProductDir') DO SET VC8DIR=%%B

ECHO Visual C++ 6 directory: %VC6DIR%
ECHO Visual C++ 7.1 directory: %VC71DIR%
ECHO Visual C++ 8 directory: %VC8DIR%
