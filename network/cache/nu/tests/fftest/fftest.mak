# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

!IF "$(CFG)" == ""
CFG=fftest - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to fftest - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "fftest - Win32 Release" && "$(CFG)" != "fftest - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "fftest.mak" CFG="fftest - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "fftest - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "fftest - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "fftest - Win32 Debug"
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "fftest - Win32 Release"

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
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\fftest.exe"

CLEAN : 
	-@erase "$(INTDIR)\FlatFileTest.obj"
	-@erase "$(INTDIR)\nsFFEntry.obj"
	-@erase "$(INTDIR)\nsFFObject.obj"
	-@erase "$(INTDIR)\nsFlatFile.obj"
	-@erase "$(INTDIR)\nsTOC.obj"
	-@erase "$(OUTDIR)\fftest.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /W4 /GX /O2 /I "..\..\..\..\..\dist\win32_d.obj\include" /I "..\..\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
CPP_PROJ=/nologo /ML /W4 /GX /O2 /I "..\..\..\..\..\dist\win32_d.obj\include"\
 /I "..\..\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE"\
 /Fp"$(INTDIR)/fftest.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/fftest.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 ..\..\..\..\..\dist\win32_d.obj\lib\nspr3.lib ..\..\..\..\..\dist\win32_d.obj\lib\plc3.lib /nologo /subsystem:console /machine:I386
LINK32_FLAGS=..\..\..\..\..\dist\win32_d.obj\lib\nspr3.lib\
 ..\..\..\..\..\dist\win32_d.obj\lib\plc3.lib /nologo /subsystem:console\
 /incremental:no /pdb:"$(OUTDIR)/fftest.pdb" /machine:I386\
 /out:"$(OUTDIR)/fftest.exe" 
LINK32_OBJS= \
	"$(INTDIR)\FlatFileTest.obj" \
	"$(INTDIR)\nsFFEntry.obj" \
	"$(INTDIR)\nsFFObject.obj" \
	"$(INTDIR)\nsFlatFile.obj" \
	"$(INTDIR)\nsTOC.obj"

"$(OUTDIR)\fftest.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "fftest - Win32 Debug"

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
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\fftest.exe"

CLEAN : 
	-@erase "$(INTDIR)\FlatFileTest.obj"
	-@erase "$(INTDIR)\nsFFEntry.obj"
	-@erase "$(INTDIR)\nsFFObject.obj"
	-@erase "$(INTDIR)\nsFlatFile.obj"
	-@erase "$(INTDIR)\nsTOC.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\fftest.exe"
	-@erase "$(OUTDIR)\fftest.ilk"
	-@erase "$(OUTDIR)\fftest.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /W4 /Gm /GX /Zi /Od /I "..\..\..\..\..\dist\win32_d.obj\include" /I "..\..\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
CPP_PROJ=/nologo /MLd /W4 /Gm /GX /Zi /Od /I\
 "..\..\..\..\..\dist\win32_d.obj\include" /I "..\..\include" /D "WIN32" /D\
 "_DEBUG" /D "_CONSOLE" /Fp"$(INTDIR)/fftest.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/fftest.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 ..\..\..\..\..\dist\win32_d.obj\lib\nspr3.lib ..\..\..\..\..\dist\win32_d.obj\lib\plc3.lib /nologo /subsystem:console /debug /machine:I386
LINK32_FLAGS=..\..\..\..\..\dist\win32_d.obj\lib\nspr3.lib\
 ..\..\..\..\..\dist\win32_d.obj\lib\plc3.lib /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)/fftest.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/fftest.exe" 
LINK32_OBJS= \
	"$(INTDIR)\FlatFileTest.obj" \
	"$(INTDIR)\nsFFEntry.obj" \
	"$(INTDIR)\nsFFObject.obj" \
	"$(INTDIR)\nsFlatFile.obj" \
	"$(INTDIR)\nsTOC.obj"

"$(OUTDIR)\fftest.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "fftest - Win32 Release"
# Name "fftest - Win32 Debug"

!IF  "$(CFG)" == "fftest - Win32 Release"

!ELSEIF  "$(CFG)" == "fftest - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\FlatFileTest.cpp
DEP_CPP_FLATF=\
	"..\..\..\..\..\dist\win32_d.obj\include\obsolete\protypes.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\plstr.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prcpucfg.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinet.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinrval.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prio.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prlong.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtime.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtypes.h"\
	"..\..\include\nsFFEntry.h"\
	"..\..\include\nsFFObject.h"\
	"..\..\include\nsFlatFile.h"\
	"..\..\include\nsTOC.h"\
	{$(INCLUDE)}"\sys\types.h"\
	
NODEP_CPP_FLATF=\
	"..\..\..\..\..\dist\win32_d.obj\include\macsocket.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\protypes.h"\
	

"$(INTDIR)\FlatFileTest.obj" : $(SOURCE) $(DEP_CPP_FLATF) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=\work\mozilla\network\cache\nu\src\nsFlatFile.cpp
DEP_CPP_NSFLA=\
	"..\..\..\..\..\dist\win32_d.obj\include\obsolete\protypes.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\plstr.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prcpucfg.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinet.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinrval.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prio.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prlong.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prmem.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtime.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtypes.h"\
	"..\..\include\nsFlatFile.h"\
	{$(INCLUDE)}"\sys\types.h"\
	
NODEP_CPP_NSFLA=\
	"..\..\..\..\..\dist\win32_d.obj\include\macsocket.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\protypes.h"\
	

"$(INTDIR)\nsFlatFile.obj" : $(SOURCE) $(DEP_CPP_NSFLA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\work\mozilla\network\cache\nu\src\nsFFObject.cpp
DEP_CPP_NSFFO=\
	"..\..\..\..\..\dist\win32_d.obj\include\obsolete\protypes.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prcpucfg.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtypes.h"\
	"..\..\include\nsFFObject.h"\
	
NODEP_CPP_NSFFO=\
	"..\..\..\..\..\dist\win32_d.obj\include\protypes.h"\
	

"$(INTDIR)\nsFFObject.obj" : $(SOURCE) $(DEP_CPP_NSFFO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\work\mozilla\network\cache\nu\src\nsTOC.cpp
DEP_CPP_NSTOC=\
	"..\..\..\..\..\dist\win32_d.obj\include\obsolete\protypes.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\plstr.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prcpucfg.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinet.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinrval.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prio.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prlog.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prlong.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtime.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtypes.h"\
	"..\..\include\nsFFEntry.h"\
	"..\..\include\nsFFObject.h"\
	"..\..\include\nsFlatFile.h"\
	"..\..\include\nsTOC.h"\
	{$(INCLUDE)}"\sys\types.h"\
	
NODEP_CPP_NSTOC=\
	"..\..\..\..\..\dist\win32_d.obj\include\macsocket.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\protypes.h"\
	

"$(INTDIR)\nsTOC.obj" : $(SOURCE) $(DEP_CPP_NSTOC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\work\mozilla\network\cache\nu\src\nsFFEntry.cpp
DEP_CPP_NSFFE=\
	"..\..\..\..\..\dist\win32_d.obj\include\obsolete\protypes.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prcpucfg.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prlog.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prmem.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtypes.h"\
	"..\..\include\nsFFEntry.h"\
	"..\..\include\nsFFObject.h"\
	
NODEP_CPP_NSFFE=\
	"..\..\..\..\..\dist\win32_d.obj\include\protypes.h"\
	

"$(INTDIR)\nsFFEntry.obj" : $(SOURCE) $(DEP_CPP_NSFFE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
# End Project
################################################################################
