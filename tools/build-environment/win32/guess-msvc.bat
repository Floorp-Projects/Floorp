REM -*- Mode: fundamental; tab-width: 8; indent-tabs-mode: 1 -*-
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
SET MSVC8EXPRESSKEY=HKLM\SOFTWARE\Microsoft\VCExpress\8.0\Setup\VC

REM First see if we can find MSVC, then set the variable
REM NOTE: delims=<tab><space>
REM NOTE: run the initial REQ QUERY outside of the if() to set ERRORLEVEL correctly

REG QUERY "%MSVC6KEY%" /v ProductDir >nul 2>nul
if "%VC6DIR%"=="" (
  IF %ERRORLEVEL% EQU 0 (
    FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "%MSVC6KEY%" /v ProductDir') DO SET VC6DIR=%%B
  )
)

REG QUERY "%MSVC71KEY%" /v ProductDir >nul 2>nul
if "%VC71DIR%"=="" (
  IF %ERRORLEVEL% EQU 0 (
    FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "%MSVC71KEY%" /v ProductDir') DO SET VC71DIR=%%B
  )
)

REG QUERY "%MSVC8KEY%" /v ProductDir >nul 2>nul
if "%VC8DIR%"=="" (
  IF %ERRORLEVEL% EQU 0 (
    FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "%MSVC8KEY%" /v ProductDir') DO SET VC8DIR=%%B
  )
)

REG QUERY "%MSVC8EXPRESSKEY%" /v ProductDir >nul 2>nul
if "%VC8EXPRESSDIR%"=="" (
  IF %ERRORLEVEL% EQU 0 (
    FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "%MSVC8EXPRESSKEY%" /v ProductDir') DO SET VC8EXPRESSDIR=%%B
  )
)

REM Look for Installed SDKs:
SET SDKROOTKEY=HKLM\SOFTWARE\Microsoft\MicrosoftSDK\InstalledSDKs
SET SDK2003SP1KEY=%SDKROOTKEY%\8F9E5EF3-A9A5-491B-A889-C58EFFECE8B3
SET SDK2003SP2KEY=%SDKROOTKEY%\D2FF9F89-8AA2-4373-8A31-C838BF4DBBE1

REG QUERY "%SDK2003SP2KEY%" /v "Install Dir" >nul 2>nul
if "%SDKDIR%"=="" (
  IF %ERRORLEVEL% EQU 0 (
    FOR /F "tokens=3* delims=	 " %%A IN ('REG QUERY "%SDK2003SP2KEY%" /v "Install Dir"') DO SET SDKDIR=%%B
  )
)

REG QUERY "%SDK2003SP1KEY%" /v "Install Dir" >nul 2>nul
if "%SDKDIR%"=="" (
  IF %ERRORLEVEL% EQU 0 (
    FOR /F "tokens=3* delims=	 " %%A IN ('REG QUERY "%SDK2003SP1KEY%" /v "Install Dir"') DO SET SDKDIR=%%B
  )
)

ECHO Visual C++ 6 directory: %VC6DIR%
ECHO Visual C++ 7.1 directory: %VC71DIR%
ECHO Visual C++ 8 directory: %VC8DIR%
ECHO Visual C++ 8 Express directory: %VC8EXPRESSDIR%
ECHO SDK directory: %SDKDIR%
