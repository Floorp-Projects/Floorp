@if not "%echo%" == "on" echo off

setlocal

REM
REM  This runs the specified test with the correct path to perl.
REM  Fills in the TEMP variable
REM
REM  Usage: mstone testname [arguments]
REM

if "%1" == "" goto Usage

perl\bin\perl -Ibin -- bin\mailmaster.pl -w conf\%1.wld TEMPDIR=%TEMP% -z %2 %3 %4 %5 %6 %7 %8 %9

goto end

:Usage

echo.
echo Usage: %0 testname [arguments]
echo.
echo        where testname is one of the .wld files in the conf\ subdirectory
goto end

:end

echo.

endlocal

