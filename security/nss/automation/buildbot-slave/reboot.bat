IF EXIST ..\buildbot-is-building (
    del ..\buildbot-is-building
    shutdown /r /t 0

    timeout /t 120
)
