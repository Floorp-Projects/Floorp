# Microsoft Developer Studio Generated NMAKE File, Based on mailclient.dsp
!IF "$(CFG)" == ""
CFG=mailclient - Win32 Debug
!MESSAGE No configuration specified. Defaulting to mailclient - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "mailclient - Win32 Release" && "$(CFG)" != "mailclient - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mailclient.mak" CFG="mailclient - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mailclient - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mailclient - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "mailclient - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\built\WINNT4.0_OPT.OBJ\mailclient.exe"


CLEAN :
	-@erase "$(INTDIR)\bench.obj"
	-@erase "$(INTDIR)\client.obj"
	-@erase "$(INTDIR)\errexit.obj"
	-@erase "$(INTDIR)\getopt.obj"
	-@erase "$(INTDIR)\http.obj"
	-@erase "$(INTDIR)\imap4.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\parse.obj"
	-@erase "$(INTDIR)\pop3.obj"
	-@erase "$(INTDIR)\smtp.obj"
	-@erase "$(INTDIR)\sysdep.obj"
	-@erase "$(INTDIR)\timefunc.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\wmap.obj"
	-@erase "..\built\WINNT4.0_OPT.OBJ\mailclient.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /I /D "VERSION=\" 4.1\"" /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mailclient.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\mailclient.pdb" /machine:I386 /out:"..\built\WINNT4.0_OPT.OBJ\mailclient.exe" 
LINK32_OBJS= \
	"$(INTDIR)\bench.obj" \
	"$(INTDIR)\client.obj" \
	"$(INTDIR)\errexit.obj" \
	"$(INTDIR)\getopt.obj" \
	"$(INTDIR)\http.obj" \
	"$(INTDIR)\imap4.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\parse.obj" \
	"$(INTDIR)\pop3.obj" \
	"$(INTDIR)\smtp.obj" \
	"$(INTDIR)\sysdep.obj" \
	"$(INTDIR)\timefunc.obj" \
	"$(INTDIR)\wmap.obj"

"..\built\WINNT4.0_OPT.OBJ\mailclient.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\built\WINNT4.0_DBG.OBJ\mailclient.exe" "$(OUTDIR)\mailclient.bsc"


CLEAN :
	-@erase "$(INTDIR)\bench.obj"
	-@erase "$(INTDIR)\bench.sbr"
	-@erase "$(INTDIR)\client.obj"
	-@erase "$(INTDIR)\client.sbr"
	-@erase "$(INTDIR)\errexit.obj"
	-@erase "$(INTDIR)\errexit.sbr"
	-@erase "$(INTDIR)\getopt.obj"
	-@erase "$(INTDIR)\getopt.sbr"
	-@erase "$(INTDIR)\http.obj"
	-@erase "$(INTDIR)\http.sbr"
	-@erase "$(INTDIR)\imap4.obj"
	-@erase "$(INTDIR)\imap4.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\parse.obj"
	-@erase "$(INTDIR)\parse.sbr"
	-@erase "$(INTDIR)\pop3.obj"
	-@erase "$(INTDIR)\pop3.sbr"
	-@erase "$(INTDIR)\smtp.obj"
	-@erase "$(INTDIR)\smtp.sbr"
	-@erase "$(INTDIR)\sysdep.obj"
	-@erase "$(INTDIR)\sysdep.sbr"
	-@erase "$(INTDIR)\timefunc.obj"
	-@erase "$(INTDIR)\timefunc.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\wmap.obj"
	-@erase "$(INTDIR)\wmap.sbr"
	-@erase "$(OUTDIR)\mailclient.bsc"
	-@erase "$(OUTDIR)\mailclient.pdb"
	-@erase "..\built\WINNT4.0_DBG.OBJ\mailclient.exe"
	-@erase "..\built\WINNT4.0_DBG.OBJ\mailclient.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "_WIN32" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /D "VERSION=\" 4.1\"" /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mailclient.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\bench.sbr" \
	"$(INTDIR)\client.sbr" \
	"$(INTDIR)\errexit.sbr" \
	"$(INTDIR)\getopt.sbr" \
	"$(INTDIR)\http.sbr" \
	"$(INTDIR)\imap4.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\parse.sbr" \
	"$(INTDIR)\pop3.sbr" \
	"$(INTDIR)\smtp.sbr" \
	"$(INTDIR)\sysdep.sbr" \
	"$(INTDIR)\timefunc.sbr" \
	"$(INTDIR)\wmap.sbr"

"$(OUTDIR)\mailclient.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib wsock32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\mailclient.pdb" /debug /machine:I386 /out:"..\built\WINNT4.0_DBG.OBJ\mailclient.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\bench.obj" \
	"$(INTDIR)\client.obj" \
	"$(INTDIR)\errexit.obj" \
	"$(INTDIR)\getopt.obj" \
	"$(INTDIR)\http.obj" \
	"$(INTDIR)\imap4.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\parse.obj" \
	"$(INTDIR)\pop3.obj" \
	"$(INTDIR)\smtp.obj" \
	"$(INTDIR)\sysdep.obj" \
	"$(INTDIR)\timefunc.obj" \
	"$(INTDIR)\wmap.obj"

"..\built\WINNT4.0_DBG.OBJ\mailclient.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("mailclient.dep")
!INCLUDE "mailclient.dep"
!ELSE 
!MESSAGE Warning: cannot find "mailclient.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "mailclient - Win32 Release" || "$(CFG)" == "mailclient - Win32 Debug"
SOURCE=.\bench.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\bench.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\bench.obj"	"$(INTDIR)\bench.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\client.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\client.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\client.obj"	"$(INTDIR)\client.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\errexit.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\errexit.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\errexit.obj"	"$(INTDIR)\errexit.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\getopt.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\getopt.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\getopt.obj"	"$(INTDIR)\getopt.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\http.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\http.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\http.obj"	"$(INTDIR)\http.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\imap4.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\imap4.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\imap4.obj"	"$(INTDIR)\imap4.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\main.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\parse.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\parse.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\parse.obj"	"$(INTDIR)\parse.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\pop3.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\pop3.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\pop3.obj"	"$(INTDIR)\pop3.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\smtp.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\smtp.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\smtp.obj"	"$(INTDIR)\smtp.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\sysdep.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\sysdep.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\sysdep.obj"	"$(INTDIR)\sysdep.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\timefunc.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\timefunc.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\timefunc.obj"	"$(INTDIR)\timefunc.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\wmap.c

!IF  "$(CFG)" == "mailclient - Win32 Release"


"$(INTDIR)\wmap.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "mailclient - Win32 Debug"


"$(INTDIR)\wmap.obj"	"$(INTDIR)\wmap.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 


!ENDIF 

