@if not "%echo%" == "on" echo off
REM
REM  copy program and messages to temp directory
REM

setlocal

set OS_ARCH=WINNT
set OS_RELEASE=4.0
set OS_CONFIG=%OS_ARCH%%OS_RELEASE%

if not exist perl\nul mkdir perl
if not exist perl\bin\nul mkdir perl\bin
if exist perl\bin\perl.exe goto PerlExists

if not exist perl\arch\%OS_CONFIG%\perl.exe goto NoPerl

copy perl\arch\%OS_CONFIG%\*.* perl\bin > nul

if not exist perl\bin\perl.exe goto NoPerl

:PerlExists

if not exist gd\nul mkdir gd
if exist gd\gd.html goto GDExists

if not exist bin\%OS_CONFIG%\gd\gd.html goto NoGD

copy bin\%OS_CONFIG%\gd\* gd > nul

if not exist gd\gd.html goto NoGD

:GDExists

if not exist gnuplot\nul mkdir gnuplot
if exist gnuplot\gnuplot.exe goto GnuPlotExists

if not exist bin\%OS_CONFIG%\gnuplot\gnuplot.exe goto NoGnuPlot

copy bin\%OS_CONFIG%\gnuplot\* gnuplot > nul

if not exist gnuplot\gnuplot.exe goto NoGnuPlot

:GnuPlotExists

if not exist bin\nul mkdir bin
if exist bin\mailclient.exe goto MailClientExists

if not exist bin\%OS_CONFIG%\bin\mailclient.exe goto NoMailClient

copy bin\%OS_CONFIG%\bin\mailclient.exe bin > nul

:MailClientExists

REM Mode is our name unless the first word is a known mode
set MODE=%0
REM SetMode will jump back to ModeCheckDone
if "%1" == "setup" goto SetMode
if "%1" == "cleanup" goto SetMode
if "%1" == "config" goto SetMode
if "%1" == "checktime" goto SetMode

:ModeCheckDone

REM All the OS setup is done, now make the copies for test execution

REM While debugging the perl version, just do the copies
copy data\*.msg %TEMP% > nul
copy bin\mailclient.exe %TEMP% > nul

REM Run the perl version of setup to handle license and configuration
perl\bin\perl -Ibin -- bin\setup.pl %MODE% TEMPDIR=%TEMP% -w conf\general.wld -z %1 %2 %3 %4 %5 %6 %7 %8


goto end

:SetMode

REM set the MODE to be the arg1 instead of arg0.  shift remaining args
set MODE=%1
shift
goto ModeCheckDone


:NoPerl

echo.
echo ERROR: Cannot find perl\arch\%OS_CONFIG%\perl.exe nor perl\bin\perl.exe
echo.
echo        Either your mailstone package is incomplete, or you are
echo        attempting to run setup.bat from the wrong location.
goto end

:NoGD

echo.
echo ERROR: Cannot find bin\%OS_CONFIG%\gd\gd.html nor gd\gd.html
echo.
echo        Either your mailstone package is incomplete, or you are
echo        attempting to run setup.bat from the wrong location.
goto end

:NoGnuPlot

echo.
echo ERROR: Cannot find bin\%OS_CONFIG%\gnuplot\gnuplot.exe nor gnuplot\gnuplot.exe
echo.
echo        Either your mailstone package is incomplete, or you are
echo        attempting to run setup.bat from the wrong location.
goto end

:NoMailClient

echo.
echo ERROR: Cannot find bin\%OS_CONFIG%\bin\mailclient.exe nor bin\mailclient.exe
echo.
echo        Either your mailstone package is incomplete, or you are
echo        attempting to run setup.bat from the wrong location.
goto end

:end

echo.

endlocal

