# Microsoft Developer Studio Project File - Name="cbrowse" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=cbrowse - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "cbrowse.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cbrowse.mak" CFG="cbrowse - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cbrowse - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "cbrowse - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cbrowse - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "cbrowse - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /FR /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "cbrowse - Win32 Release"
# Name "cbrowse - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\ActiveScriptSite.cpp
# End Source File
# Begin Source File

SOURCE=.\cbrowse.cpp
# End Source File
# Begin Source File

SOURCE=.\Cbrowse.idl
# ADD MTL /h "Cbrowse_i.h" /iid "Cbrowse_i.c" /Oicf
# End Source File
# Begin Source File

SOURCE=.\cbrowse.rc
# End Source File
# Begin Source File

SOURCE=.\CBrowseDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\..\ControlSite.cpp
# End Source File
# Begin Source File

SOURCE=..\..\ControlSiteIPFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\PickerDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\..\PropertyBag.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\Tests.cpp
# End Source File
# Begin Source File

SOURCE=.\TestScriptHelper.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\ActiveScriptSite.h
# End Source File
# Begin Source File

SOURCE=.\cbrowse.h
# End Source File
# Begin Source File

SOURCE=.\Cbrowse_i.h
# End Source File
# Begin Source File

SOURCE=.\CBrowseDlg.h
# End Source File
# Begin Source File

SOURCE=..\..\ControlSite.h
# End Source File
# Begin Source File

SOURCE=..\..\ControlSiteIPFrame.h
# End Source File
# Begin Source File

SOURCE=.\PickerDlg.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\Tests.h
# End Source File
# Begin Source File

SOURCE=.\TestScriptHelper.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\cbrowse.ico
# End Source File
# Begin Source File

SOURCE=.\res\cbrowse.rc2
# End Source File
# Begin Source File

SOURCE=.\res\closedfolder.ico
# End Source File
# Begin Source File

SOURCE=.\res\openfold.ico
# End Source File
# Begin Source File

SOURCE=.\res\openfolder.ico
# End Source File
# Begin Source File

SOURCE=.\res\test.ico
# End Source File
# Begin Source File

SOURCE=.\res\testfailed.ico
# End Source File
# Begin Source File

SOURCE=.\res\testpassed.ico
# End Source File
# End Group
# Begin Group "Scripts"

# PROP Default_Filter "js;vbs"
# Begin Source File

SOURCE=.\Scripts\Basic.vbs
# End Source File
# End Group
# Begin Source File

SOURCE=.\Cbrowse.rgs
# End Source File
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# Begin Source File

SOURCE=.\TestScriptHelper.rgs
# End Source File
# End Target
# End Project
# Section cbrowse : {10404710-0021-001E-1B00-1B00EAA80000}
# 	1:11:IDR_CBROWSE:103
# End Section
