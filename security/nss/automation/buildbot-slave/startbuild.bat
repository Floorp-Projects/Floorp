echo running > ..\buildbot-is-building

echo running: "%MOZILLABUILD%\msys\bin\bash" -c "hg/tinder/buildbot/build.sh %*"
"%MOZILLABUILD%\msys\bin\bash" -c "hg/tinder/buildbot/build.sh %*"

if %errorlevel% neq 0 (
  set EXITCODE=1
) else (
  set EXITCODE=0
)

del ..\buildbot-is-building

exit /b %EXITCODE%
