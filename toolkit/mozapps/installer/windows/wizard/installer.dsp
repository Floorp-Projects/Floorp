# Microsoft Developer Studio Project File - Name="installer" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=installer - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "installer.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "installer.mak" CFG="installer - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "installer - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "installer - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "installer - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "installer - Win32 Release"
# Name "installer - Win32 Debug"
# Begin Group "setup"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\windows\wizard\setup\dialogs.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\dialogs.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\extern.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\extra.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\extra.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\ifuncns.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\ifuncns.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\logging.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\logging.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\nsEscape.cpp

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\nsEscape.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\process.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\process.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\resource.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\setup.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\setup.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\setup.rc

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\shortcut.cpp

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\shortcut.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\supersede.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\supersede.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\version.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\version.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\wizverreg.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\xperr.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\xpi.c

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\xpi.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\xpistub.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\xpnetHook.cpp

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\xpnetHook.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setup\zipfile.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Ignore_Default_Tool 1

!ENDIF 

# End Source File
# End Group
# Begin Group "setuprsc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\windows\wizard\setuprsc\setuprsc.cpp

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setuprsc\setuprsc.h

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\wizard\setuprsc\setuprsc.rc
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Group "xappscripts"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\windows\build_static.pl

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\windows\makeall.pl

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\makecfgini.pl
# End Source File
# End Group
# Begin Group "browserscripts"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\abe.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\adt.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\browser.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\chatzilla.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\config.it

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\deflenus.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\editor.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\..\..\..\..\..\browser\installer\windows\firebird-win32-stub-installer.jst"

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\inspector.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\install.it

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\installer.cfg

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\langenus.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\..\..\..\..\browser\installer\windows\packages-static"
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\psm.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\redirect.it

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\regus.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\talkback.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\uninstall.it

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\venkman.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\browser\installer\windows\xpcom.jst

!IF  "$(CFG)" == "installer - Win32 Release"

!ELSEIF  "$(CFG)" == "installer - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=.\setuprsc\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\setuprsc\browser.ico
# End Source File
# Begin Source File

SOURCE=.\setuprsc\Header.bmp
# End Source File
# Begin Source File

SOURCE=.\setuprsc\setup.ico
# End Source File
# Begin Source File

SOURCE=.\setuprsc\Watermrk.bmp
# End Source File
# End Target
# End Project
