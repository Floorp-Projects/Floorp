@if not "%echo%" == "on" echo off

REM ************ simple autobuild for mailstone

setlocal

set BOTH=0

if not "%1" == "" goto GetTag

set BOTH=1
set TAG=_OPT
goto SetConfig

:GetTag

if %1 == dbg set TAG=_DBG
if %1 == DBG set TAG=_DBG
if %1 == Debug set TAG=_DBG
if %1 == debug set TAG=_DBG
if %1 == Release set TAG=_OPT
if %1 == release set TAG=_OPT
if %1 == opt set TAG=_OPT
if %1 == OPT set TAG=_OPT
if %1 == optimize set TAG=_OPT
if %1 == Optimize set TAG=_OPT
if %1 == optimized set TAG=_OPT
if %1 == Optimized set TAG=_OPT
if %1 == clean goto CleanBoth

:SetConfig

if %TAG% == _DBG set Config=Debug
if %TAG% == _OPT set Config=Release

set ARCH=WINNT4.0%TAG%.OBJ

set FINAL_PATH=built\package\%ARCH%\mailstone

if not "%2" == "clean" if not exist %FINAL_PATH%\nul mkdir %FINAL_PATH%


REM ************ first, clean the binary release
nmake /f mailstone.mak CFG="mailstone - Win32 %Config%" /nologo NO_EXTERNAL_DEPS=1 CLEAN

if not "%2" == "clean" goto BuildMailstone

if exist src\%Config%\nul echo y | rd /s src\%Config% > nul
if exist src\gnuplot-3.7\%Config%\nul echo y | rd /s src\gnuplot-3.7\%Config% > nul
if exist src\gd1.3\%Config%\nul echo y | rd /s src\gd1.3\%Config% > nul
if exist built\%ARCH%\nul echo y | rd /s built\%ARCH% > nul
if exist %FINAL_PATH%\nul echo y | rd /s %FINAL_PATH% > nul

goto done

:BuildMailstone

REM **************** next, build it
nmake /f mailstone.mak CFG="mailstone - Win32 %Config%" /nologo NO_EXTERNAL_DEPS=1
if errorlevel 1 goto BadBuild

REM ************ next, copy the top-level files
copy mstone.bat %FINAL_PATH%
copy setup.bat %FINAL_PATH%
copy CHANGELOG %FINAL_PATH%
copy INSTALL %FINAL_PATH%
copy README %FINAL_PATH%
copy LICENSE %FINAL_PATH%

REM ************ now, copy the files for running mailstone into bin
if not exist %FINAL_PATH%\bin\nul mkdir %FINAL_PATH%\bin
copy built\%ARCH%\mailclient.exe %FINAL_PATH%\bin
if exist built\package\%ARCH%\mailclient.exe copy built\package\%ARCH%\mailclient.exe %FINAL_PATH%\bin
if exist built\package\%ARCH%\mailclient.exe del /f /q built\package\%ARCH%\mailclient.exe
copy bin\*.pl %FINAL_PATH%\bin

REM ************ now, copy the configuration files into conf
if not exist %FINAL_PATH%\conf\nul mkdir %FINAL_PATH%\conf
copy conf\*.* %FINAL_PATH%\conf

REM ************ now, copy the data files into data
if not exist %FINAL_PATH%\data\nul mkdir %FINAL_PATH%\data
copy data\*.msg %FINAL_PATH%\data

REM ************ now, copy the gd files into gd
if not exist %FINAL_PATH%\gd\nul mkdir %FINAL_PATH%\gd
copy src\gd1.3\index.html %FINAL_PATH%\gd\gd.html

REM ************ now, copy the gnuplot files into gnuplot
if not exist %FINAL_PATH%\gnuplot\nul mkdir %FINAL_PATH%\gnuplot
copy built\%ARCH%\gnuplot\gnuplot.exe %FINAL_PATH%\gnuplot
copy src\gnuplot-3.7\Copyright %FINAL_PATH%\gnuplot
copy src\gnuplot-3.7\docs\gnuplot.1 %FINAL_PATH%\gnuplot

REM ************ now, copy the perl files into perl
if not exist %FINAL_PATH%\perl\nul mkdir %FINAL_PATH%\perl
if not exist %FINAL_PATH%\perl\bin\nul mkdir %FINAL_PATH%\perl\bin
if not exist %FINAL_PATH%\perl\lib\nul mkdir %FINAL_PATH%\perl\lib
if not exist %FINAL_PATH%\perl\lib\5.00503\nul mkdir %FINAL_PATH%\perl\lib\5.00503
if not exist %FINAL_PATH%\perl\lib\5.00503\MSWin32-x86\nul mkdir %FINAL_PATH%\perl\lib\5.00503\MSWin32-x86
#copy built\%ARCH%\perl\perl.exe %FINAL_PATH%\perl\bin
#rcp -b sandpit:/share/builds/components/perl5/WINNT-perl5/perl.exe %FINAL_PATH%\perl\bin\perl.exe
#if errorlevel 1 goto BadRcp
#if not exist %FINAL_PATH%\perl\perl.exe goto BadRcp
#rcp -b sandpit:/share/builds/components/perl5/WINNT-perl5/perl300.dll %FINAL_PATH%\perl\bin\perl300.dll
#if errorlevel 1 goto BadRcp
#if not exist %FINAL_PATH%\perl\perl300.dll goto BadRcp

copy src\perl5.005_03\Artistic %FINAL_PATH%\perl
copy c:\perl\5.00503\bin\MSWin32-x86\perl.exe %FINAL_PATH%\perl\bin
copy c:\perl\5.00503\bin\MSWin32-x86\perl.dll %FINAL_PATH%\perl\bin
copy c:\perl\5.00503\lib\*.pm %FINAL_PATH%\perl\lib\5.00503
copy c:\perl\5.00503\lib\*.pl %FINAL_PATH%\perl\lib\5.00503
copy c:\perl\5.00503\lib\MSWin32-x86\*.pm %FINAL_PATH%\perl\lib\5.00503\MSWin32-x86


goto end

:CleanBoth

echo.
echo NOTICE: CLEANING debug build
call autobuild.bat debug clean

echo NOTICE: CLEANING optimized build
call autobuild.bat release clean

echo NOTICE: Removing generated dependency files
del /s *.dep

if exist built\nul echo y | rd /s built > nul

goto done

:BadRcp

echo.
echo ERROR: Failed to rcp perl files over to mailstone packaging
echo ERROR: Two common causes of this are .rhosts permissions or a broken rcp.exe
echo ERROR: Make sure you are not using rcp.exe from NT4.0 SP4
echo ERROR: The SP5 version is available in \\cobra\engineering\bin\rcp_sp5.exe
echo ERROR: Use this version to replace ...\system32\rcp.exe
goto done

:BadBuild

echo.
echo ERROR: Failed to build mailstone
goto done

:end

echo.

if %BOTH% == 0 goto done
if %TAG% == _DBG goto done

set TAG=_DBG
goto SetConfig

:done

endlocal

