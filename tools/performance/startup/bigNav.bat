@mozilla.exe -P "Default User" -url chrome://navigator/content/quit.html
@mozilla.exe -P "Default User" -url chrome://navigator/content/quit.html
@grep "Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@mozilla.exe -P "Default User" -url chrome://navigator/content/quit.html
@grep "Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@mozilla.exe -P "Default User" -url chrome://navigator/content/quit.html
@grep "Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@copy "%NS_TIMELINE_LOG_FILE%" bigNav.log
