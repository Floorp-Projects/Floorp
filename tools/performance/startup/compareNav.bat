@setlocal
@if "%2"=="" @goto :error
@if "%1"=="" @goto :error
@rem Choose executable (commercial build if there is one).
@if exist netscp6.exe @goto :nav
@set prog=mozilla.exe
@goto :profile1
:nav
@set prog=netscp6.exe
@rem Set profiles, defaulting to "Default User"
:profile1
@if "%3"=="" @goto :defaultUser
@set profile1=%3
@goto :profile2
:defaultUser
@set profile1=Default User
:profile2
@if "%4"=="" @goto :defaultUser2
@set profile2=%4
@goto :start
:defaultUser2
@set profile2=%profile1%
:start
@echo ----- %1/%profile1% -----
@rem For each *.jar.<a>, save *.jar and replace it.
@for %%x in ( chrome\*.jar ) do @if exist %%x.%1 (copy %%x %%x.save >nul & copy %%x.%1 %%x >nul)
@rem Do likewise for these special files: installed-chrome.txt chrome.rdf
@for %%x in ( chrome\installed-chrome.txt chrome\chrome.rdf ) do @if exist %%x.%1 (copy %%x %%x.save >nul & copy %%x.%1 %%x >nul)
@%prog% -P "%profile1%" -url chrome://navigator/content/quit.html > nul
@%prog% -P "%profile1%" -url chrome://navigator/content/quit.html > nul
@grep \"Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@%prog% -P "%profile1%" -url chrome://navigator/content/quit.html > nul
@grep \"Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@%prog% -P "%profile1%" -url chrome://navigator/content/quit.html > nul
@grep \"Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@copy "%NS_TIMELINE_LOG_FILE%" "%1.%profile1%.log" > nul
@rem Restore *.jar files.
@for %%x in ( chrome\*.jar ) do @if exist %%x.%1 @(copy %%x.save %%x >nul & del %%x.save)
@rem Restore special files.
@for %%x in ( chrome\installed-chrome.txt chrome\chrome.rdf ) do @if exist %%x.%1 @(copy %%x.save %%x >nul & del %%x.save)
:skip1
@echo ----- %2/%profile2% -----
@rem For each *.jar.<b>, save *.jar and replace it.
@for %%x in ( chrome\*.jar ) do @if exist %%x.%2 @(copy %%x %%x.save >nul & copy %%x.%2 %%x >nul)
@rem Do likewise for these special files: installed-chrome.txt chrome.rdf
@for %%x in ( chrome\installed-chrome.txt chrome\chrome.rdf ) do @if exist %%x.%2 (copy %%x %%x.save >nul & copy %%x.%2 %%x >nul)
@%prog% -P "%profile2%" -url chrome://navigator/content/quit.html > nul
@%prog% -P "%profile2%" -url chrome://navigator/content/quit.html > nul
@grep \"Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@%prog% -P "%profile2%" -url chrome://navigator/content/quit.html > nul
@grep \"Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@%prog% -P "%profile2%" -url chrome://navigator/content/quit.html > nul
@grep \"Navigator Window visible now" "%NS_TIMELINE_LOG_FILE%"
@copy "%NS_TIMELINE_LOG_FILE%" "%2.%profile2%.log" > nul
@rem Restore *.jar files.
@for %%x in ( chrome\*.jar ) do @if exist %%x.%2 @(copy %%x.save %%x >nul & del %%x.save)
@rem Restore special files.
@for %%x in ( chrome\installed-chrome.txt chrome\chrome.rdf ) do @if exist %%x.%2 @(copy %%x.save %%x >nul & del %%x.save)
:skip2
@goto :done
:error
@echo Syntax: compareNav ^<a^> ^<b^> ^<profile1^> ^<profile2^>
@echo     where ^<a^> and ^<b^> are flavors of comm.jar
@echo           e.g., comm.jar.^<a^> and comm.jar.^<b^>
@echo     and   ^<profile1^> and ^<profile2^> are optional profile names
@echo           to use; e.g., "classic" or "modern"; profile1 defaults to
@echo           "Default User", profile2 defaults to profile1
@goto :done
:done
