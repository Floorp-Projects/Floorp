# Microsoft Developer Studio Generated NMAKE File, Based on mailstone.dsp
!IF "$(CFG)" == ""
CFG=mailstone - Win32 Debug
!MESSAGE No configuration specified. Defaulting to mailstone - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "mailstone - Win32 Release" && "$(CFG)" != "mailstone - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mailstone.mak" CFG="mailstone - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mailstone - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mailstone - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mailstone - Win32 Release"

OUTDIR=.\built\WINNT4.0_OPT.OBJ
INTDIR=.\built\WINNT4.0_OPT.OBJ

!IF "$(RECURSE)" == "0" 

ALL : 

!ELSE 

ALL : "gnuplot - Win32 Release" "mailclient - Win32 Release" 

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"mailclient - Win32 ReleaseCLEAN" "gnuplot - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@if exist $(INTDIR)\postbld.dep erase $(INTDIR)\postbld.dep
	-@if exist built\package\WINNT4.0_OPT.OBJ\mailclient.exe erase built\package\WINNT4.0_OPT.OBJ\mailclient.exe

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mailstone.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\mailstone.pdb" /machine:I386 /out:"$(OUTDIR)\mailstone.exe" 
LINK32_OBJS= \
	
OutDir=.\built\WINNT4.0_OPT.OBJ
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

$(DS_POSTBUILD_DEP) : "gnuplot - Win32 Release" "mailclient - Win32 Release" 
   if not exist built\package\WINNT4.0_OPT.OBJ\nul mkdir built\package\WINNT4.0_OPT.OBJ
	copy .\built\WINNT4.0_OPT.OBJ\mailclient.exe built\package\WINNT4.0_OPT.OBJ
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "mailstone - Win32 Debug"

OUTDIR=.\built\WINNT4.0_DBG.OBJ
INTDIR=.\built\WINNT4.0_DBG.OBJ

!IF "$(RECURSE)" == "0" 

ALL : 

!ELSE 

ALL : "gnuplot - Win32 Debug" "mailclient - Win32 Debug" 

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"mailclient - Win32 DebugCLEAN" "gnuplot - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@if exist $(INTDIR)\postbld.dep erase /q $(INTDIR)\postbld.dep
	-@if exist built\package\WINNT4.0_DBG.OBJ\mailclient.exe erase /q built\package\WINNT4.0_DBG.OBJ\mailclient.exe

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_WIN32" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mailstone.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\mailstone.pdb" /debug /machine:I386 /out:"$(OUTDIR)\mailstone.exe" /pdbtype:sept 
LINK32_OBJS= \
	
OutDir=.\built\WINNT4.0_DBG.OBJ
SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

$(DS_POSTBUILD_DEP) : "gnuplot - Win32 Debug" "mailclient - Win32 Debug" 
   if not exist built\package\WINNT4.0_DBG.OBJ\nul mkdir built\package\WINNT4.0_DBG.OBJ
	copy .\built\WINNT4.0_DBG.OBJ\mailclient.exe built\package\WINNT4.0_DBG.OBJ
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("mailstone.dep")
!INCLUDE "mailstone.dep"
!ELSE 
!MESSAGE Warning: cannot find "mailstone.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "mailstone - Win32 Release" || "$(CFG)" == "mailstone - Win32 Debug"

!IF  "$(CFG)" == "mailstone - Win32 Release"

"mailclient - Win32 Release" : 
   cd ".\src"
   $(MAKE) /$(MAKEFLAGS) /F .\mailclient.mak CFG="mailclient - Win32 Release" 
   cd ".."

"mailclient - Win32 ReleaseCLEAN" : 
   cd ".\src"
   $(MAKE) /$(MAKEFLAGS) /F .\mailclient.mak CFG="mailclient - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "mailstone - Win32 Debug"

"mailclient - Win32 Debug" : 
   cd ".\src"
   $(MAKE) /$(MAKEFLAGS) /F .\mailclient.mak CFG="mailclient - Win32 Debug" 
   cd ".."

"mailclient - Win32 DebugCLEAN" : 
   cd ".\src"
   $(MAKE) /$(MAKEFLAGS) /F .\mailclient.mak CFG="mailclient - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "mailstone - Win32 Release"

"gnuplot - Win32 Release" : 
   cd ".\src\gnuplot-3.7"
   $(MAKE) /$(MAKEFLAGS) /F ".\gnuplot.mak" CFG="gnuplot - Win32 Release" 
   cd "..\.."

"gnuplot - Win32 ReleaseCLEAN" : 
   cd ".\src\gnuplot-3.7"
   $(MAKE) /$(MAKEFLAGS) /F ".\gnuplot.mak" CFG="gnuplot - Win32 Release" RECURSE=1 CLEAN 
   cd "..\.."

!ELSEIF  "$(CFG)" == "mailstone - Win32 Debug"

"gnuplot - Win32 Debug" : 
   cd ".\src\gnuplot-3.7"
   $(MAKE) /$(MAKEFLAGS) /F ".\gnuplot.mak" CFG="gnuplot - Win32 Debug" 
   cd "..\.."

"gnuplot - Win32 DebugCLEAN" : 
   cd ".\src\gnuplot-3.7"
   $(MAKE) /$(MAKEFLAGS) /F ".\gnuplot.mak" CFG="gnuplot - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\.."

!ENDIF 


!ENDIF 

