# Microsoft Developer Studio Project File - Name="libical" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libical - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libical.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libical.mak" CFG="libical - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libical - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libical - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libical - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libical - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libical - Win32 Release"
# Name "libical - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\caldate.c
# End Source File
# Begin Source File

SOURCE=.\icalarray.c
# End Source File
# Begin Source File

SOURCE=icalattendee.c
# End Source File
# Begin Source File

SOURCE=icalcomponent.c
# End Source File
# Begin Source File

SOURCE=icalderivedparameter.c
# End Source File
# Begin Source File

SOURCE=icalderivedparameter.c.in
# End Source File
# Begin Source File

SOURCE=icalderivedproperty.c
# End Source File
# Begin Source File

SOURCE=icalderivedproperty.c.in
# End Source File
# Begin Source File

SOURCE=icalderivedvalue.c
# End Source File
# Begin Source File

SOURCE=icalderivedvalue.c.in
# End Source File
# Begin Source File

SOURCE=icalduration.c
# End Source File
# Begin Source File

SOURCE=icalenums.c
# End Source File
# Begin Source File

SOURCE=icalerror.c
# End Source File
# Begin Source File

SOURCE=icallangbind.c
# End Source File
# Begin Source File

SOURCE=icallexer.c
# End Source File
# Begin Source File

SOURCE=icalmemory.c
# End Source File
# Begin Source File

SOURCE=icalmime.c
# End Source File
# Begin Source File

SOURCE=icalparameter.c
# End Source File
# Begin Source File

SOURCE=icalparser.c
# End Source File
# Begin Source File

SOURCE=icalperiod.c
# End Source File
# Begin Source File

SOURCE=icalproperty.c
# End Source File
# Begin Source File

SOURCE=icalrecur.c
# End Source File
# Begin Source File

SOURCE=icalrestriction.c
# End Source File
# Begin Source File

SOURCE=icalrestriction.c.in
# End Source File
# Begin Source File

SOURCE=icaltime.c
# End Source File
# Begin Source File

SOURCE=.\icaltimezone.c
# End Source File
# Begin Source File

SOURCE=icaltypes.c
# End Source File
# Begin Source File

SOURCE=icalvalue.c
# End Source File
# Begin Source File

SOURCE=icalyacc.c
# End Source File
# Begin Source File

SOURCE=pvl.c
# End Source File
# Begin Source File

SOURCE=sspm.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\astime.h
# End Source File
# Begin Source File

SOURCE=ical.h
# End Source File
# Begin Source File

SOURCE=.\icalarray.h
# End Source File
# Begin Source File

SOURCE=icalattendee.h
# End Source File
# Begin Source File

SOURCE=icalcomponent.h
# End Source File
# Begin Source File

SOURCE=icalderivedparameter.h
# End Source File
# Begin Source File

SOURCE=icalderivedparameter.h.in
# End Source File
# Begin Source File

SOURCE=icalderivedproperty.h
# End Source File
# Begin Source File

SOURCE=icalderivedproperty.h.in
# End Source File
# Begin Source File

SOURCE=icalderivedvalue.h
# End Source File
# Begin Source File

SOURCE=icalderivedvalue.h.in
# End Source File
# Begin Source File

SOURCE=icalduration.h
# End Source File
# Begin Source File

SOURCE=icalenums.h
# End Source File
# Begin Source File

SOURCE=icalerror.h
# End Source File
# Begin Source File

SOURCE=icallangbind.h
# End Source File
# Begin Source File

SOURCE=icalmemory.h
# End Source File
# Begin Source File

SOURCE=icalmime.h
# End Source File
# Begin Source File

SOURCE=icalparameter.h
# End Source File
# Begin Source File

SOURCE=icalparameterimpl.h
# End Source File
# Begin Source File

SOURCE=icalparser.h
# End Source File
# Begin Source File

SOURCE=icalperiod.h
# End Source File
# Begin Source File

SOURCE=icalproperty.h
# End Source File
# Begin Source File

SOURCE=icalrecur.h
# End Source File
# Begin Source File

SOURCE=icalrestriction.h
# End Source File
# Begin Source File

SOURCE=icaltime.h
# End Source File
# Begin Source File

SOURCE=.\icaltimezone.h
# End Source File
# Begin Source File

SOURCE=icaltypes.h
# End Source File
# Begin Source File

SOURCE=icalvalue.h
# End Source File
# Begin Source File

SOURCE=icalvalueimpl.h
# End Source File
# Begin Source File

SOURCE=icalversion.h.in
# End Source File
# Begin Source File

SOURCE=icalyacc.h
# End Source File
# Begin Source File

SOURCE=pvl.h
# End Source File
# Begin Source File

SOURCE=sspm.h
# End Source File
# End Group
# End Target
# End Project
