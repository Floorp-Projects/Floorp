@if not "%echo%" == "on" echo off

setlocal

REM
REM  This a processes a previous test with the correct path to perl.
REM
REM  Usage: process TIMESTAMP [arguments]
REM

if "%1" == "" goto Usage

perl\bin\perl -Ibin -- bin/process.pl -c results\%1\config.cfg %2 %3 %4 %5 %6 %7 %8 %9

goto end

:Usage

echo.
echo Usage: %0 TIMESTAMP [arguments]
echo.
echo        where TIMESTAMP is one of the directories in the results\ subdirectory
goto end

:end

echo.

endlocal

