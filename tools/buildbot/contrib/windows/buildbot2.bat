@echo off
rem This is Windows helper batch file for Buildbot
rem NOTE: You will need Windows NT5/XP to use some of the syntax here.

rem Please note you must have Twisted Matrix installed to use this build system
rem Details: http://twistedmatrix.com/ (Version 1.3.0 or more, preferrably 2.0+)

rem NOTE: --reactor=win32 argument is need because of Twisted
rem The Twisted default reactor is select based (ie. posix) (why?!)

rem Keep environmental settings local to this file
setlocal

rem Change the following settings to suite your environment

rem This is where you want Buildbot installed
set BB_DIR=z:\Tools\PythonLibs

rem Assuming you have TortoiseCVS installed [for CVS.exe].
set CVS_EXE="c:\Program Files\TortoiseCVS\cvs.exe"

rem Trial: --spew will give LOADS of information. Use -o for verbose.
set TRIAL=python C:\Python23\scripts\trial.py -o --reactor=win32
set BUILDBOT_TEST_VC=c:\temp

if "%1"=="helper" (
	goto print_help
)

if "%1"=="bbinstall" (
	rem You will only need to run this when you install Buildbot
	echo BB: Install BuildBot at the location you set in the config:
	echo BB: BB_DIR= %BB_DIR%
	echo BB: You must be in the buildbot-x.y.z directory to run this:
	python setup.py install --prefix %BB_DIR% --install-lib %BB_DIR%
	goto end
)

if "%1"=="cvsco" (
	echo BB: Getting Buildbot from Sourceforge CVS [if CVS in path].
	if "%2"=="" (
		echo BB ERROR: Please give a root path for the check out, eg. z:\temp
		goto end
	)

	cd %2
	echo BB: Hit return as there is no password
	%CVS_EXE% -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/buildbot login 
	%CVS_EXE% -z3 -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/buildbot co -P buildbot 
	goto end
)

if "%1"=="cvsup" (
	echo BB: Updating Buildbot from Sourceforge CVS [if CVS in path].
	echo BB: Make sure you have the project checked out in local VCS.
	
	rem we only want buildbot code, the rest is from the install
	cd %BB_DIR%
	echo BB: Hit return as there is no password
	%CVS_EXE% -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/buildbot login 
	%CVS_EXE% -z3 -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/buildbot up -P -d buildbot buildbot/buildbot
	goto end
)

if "%1"=="test" (
	rem Trial is a testing framework supplied by the Twisted Matrix package.
	rem It installs itself in the Python installation directory in a "scripts" folder,
	rem e.g. c:\python23\scripts
	rem This is just a convenience function because that directory is not in our path.
		
	if "%2" NEQ "" (
		echo BB: TEST: buildbot.test.%2
		%TRIAL% -m buildbot.test.%2
	) else (
		echo BB: Running ALL buildbot tests...
		%TRIAL% buildbot.test
	)
	goto end
)

rem Okay, nothing that we recognised to pass to buildbot
echo BB: Running buildbot...
python -c "from buildbot.scripts import runner; runner.run()" %*
goto end

:print_help
echo Buildbot helper script commands:
echo	helper		This help message
echo	test		Test buildbot is set up correctly
echo Maintenance:
echo	bbinstall	Install Buildbot from package
echo	cvsup		Update from cvs
echo	cvsco [dir]	Check buildbot out from cvs into [dir]

:end
rem End environment scope
endlocal

