# Microsoft Developer Studio Project File - Name="MozillaControl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=MozillaControl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MozillaControl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MozillaControl.mak" CFG="MozillaControl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MozillaControl - Win32 Release" (based on\
 "Win32 (x86) External Target")
!MESSAGE "MozillaControl - Win32 Debug" (based on\
 "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "MozillaControl - Win32 Release"

# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "MozillaControl.exe"
# PROP BASE Bsc_Name "MozillaControl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "MozillaControl.exe"
# PROP Bsc_Name "MozillaControl.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "MozillaControl - Win32 Debug"

# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "MozillaControl.exe"
# PROP BASE Bsc_Name "MozillaControl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "NMAKE /f makefile.win"
# PROP Rebuild_Opt "/a"
# PROP Target_File ".\WIN32_D.OBJ\npmozctl.dll"
# PROP Bsc_Name "MozillaControl.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "MozillaControl - Win32 Release"
# Name "MozillaControl - Win32 Debug"

!IF  "$(CFG)" == "MozillaControl - Win32 Release"

!ELSEIF  "$(CFG)" == "MozillaControl - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "*.cpp,*.c,*.idl,*.rc"
# Begin Source File

SOURCE=.\ActiveXPlugin.cpp
# End Source File
# Begin Source File

SOURCE=.\ActiveXPluginInstance.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlSite.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlSiteIPFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlDocument.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElement.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElementCollection.cpp
# End Source File
# Begin Source File

SOURCE=.\LegacyPlugin.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.def
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.idl
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.rc
# End Source File
# Begin Source File

SOURCE=.\npmozctl.def
# End Source File
# Begin Source File

SOURCE=.\npwin.cpp
# End Source File
# Begin Source File

SOURCE=.\nsSetupRegistry.cpp
# End Source File
# Begin Source File

SOURCE=.\PropertyBag.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\WebShellContainer.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ActiveXPlugin.h
# End Source File
# Begin Source File

SOURCE=.\ActiveXPluginInstance.h
# End Source File
# Begin Source File

SOURCE=.\BrowserDiagnostics.h
# End Source File
# Begin Source File

SOURCE=.\ControlSite.h
# End Source File
# Begin Source File

SOURCE=.\ControlSiteIPFrame.h
# End Source File
# Begin Source File

SOURCE=.\CPMozillaControl.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlDocument.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElement.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElementCollection.h
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.h
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.h
# End Source File
# Begin Source File

SOURCE=.\PropertyBag.h
# End Source File
# Begin Source File

SOURCE=.\PropertyList.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\WebShellContainer.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\MozillaBrowser.ico
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.rgs
# End Source File
# End Group
# Begin Source File

SOURCE=.\makefile.win
# End Source File
# End Target
# End Project
