#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

# Microsoft Developer Studio Generated NMAKE File, Format Version 4.10
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=libpref - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to libpref - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "libpref - Win32 Release" && "$(CFG)" !=\
 "libpref - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "libpref.mak" CFG="libpref - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libpref - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libpref - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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
# PROP Target_Last_Scanned "libpref - Win32 Debug"
MTL=mktyplib.exe
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libpref - Win32 Release"

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

ALL : "$(OUTDIR)\libpref.dll"

CLEAN : 
	-@erase "$(INTDIR)\alloca.obj"
	-@erase "$(INTDIR)\mo_array.obj"
	-@erase "$(INTDIR)\mo_atom.obj"
	-@erase "$(INTDIR)\mo_bcode.obj"
	-@erase "$(INTDIR)\mo_bool.obj"
	-@erase "$(INTDIR)\mo_cntxt.obj"
	-@erase "$(INTDIR)\mo_date.obj"
	-@erase "$(INTDIR)\mo_debug.obj"
	-@erase "$(INTDIR)\mo_emit.obj"
	-@erase "$(INTDIR)\mo_fun.obj"
	-@erase "$(INTDIR)\mo_java.obj"
	-@erase "$(INTDIR)\mo_link.obj"
	-@erase "$(INTDIR)\mo_math.obj"
	-@erase "$(INTDIR)\mo_num.obj"
	-@erase "$(INTDIR)\mo_obj.obj"
	-@erase "$(INTDIR)\mo_parse.obj"
	-@erase "$(INTDIR)\mo_scan.obj"
	-@erase "$(INTDIR)\mo_scope.obj"
	-@erase "$(INTDIR)\mo_str.obj"
	-@erase "$(INTDIR)\mocha.obj"
	-@erase "$(INTDIR)\mochaapi.obj"
	-@erase "$(INTDIR)\mochalib.obj"
	-@erase "$(INTDIR)\prefapi.obj"
	-@erase "$(INTDIR)\prefdll.res"
	-@erase "$(OUTDIR)\libpref.dll"
	-@erase "$(OUTDIR)\libpref.exp"
	-@erase "$(OUTDIR)\libpref.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "y:\ns\include" /I "y:\ns\mocha\include" /I "y:\ns\nspr\include" /I "y:\ns\dist\win32_d.obj\include\nspr" /I "y:\ns\sun-java\include" /I "y:\ns\sun-java\md-include" /I "y:\ns\cmd\winfe" /I "y:\ns\modules\libpref\src" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "XP_PC" /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "y:\ns\include" /I "y:\ns\mocha\include" /I\
 "y:\ns\nspr\include" /I "y:\ns\dist\win32_d.obj\include\nspr" /I\
 "y:\ns\sun-java\include" /I "y:\ns\sun-java\md-include" /I "y:\ns\cmd\winfe" /I\
 "y:\ns\modules\libpref\src" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "XP_PC"\
 /Fp"$(INTDIR)/libpref.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/prefdll.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/libpref.bsc" 
BSC32_SBRS= \

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/libpref.pdb" /machine:I386 /def:".\prefdll.def"\
 /out:"$(OUTDIR)/libpref.dll" /implib:"$(OUTDIR)/libpref.lib" 
DEF_FILE= \
	".\prefdll.def"
LINK32_OBJS= \
	"$(INTDIR)\alloca.obj" \
	"$(INTDIR)\mo_array.obj" \
	"$(INTDIR)\mo_atom.obj" \
	"$(INTDIR)\mo_bcode.obj" \
	"$(INTDIR)\mo_bool.obj" \
	"$(INTDIR)\mo_cntxt.obj" \
	"$(INTDIR)\mo_date.obj" \
	"$(INTDIR)\mo_debug.obj" \
	"$(INTDIR)\mo_emit.obj" \
	"$(INTDIR)\mo_fun.obj" \
	"$(INTDIR)\mo_java.obj" \
	"$(INTDIR)\mo_link.obj" \
	"$(INTDIR)\mo_math.obj" \
	"$(INTDIR)\mo_num.obj" \
	"$(INTDIR)\mo_obj.obj" \
	"$(INTDIR)\mo_parse.obj" \
	"$(INTDIR)\mo_scan.obj" \
	"$(INTDIR)\mo_scope.obj" \
	"$(INTDIR)\mo_str.obj" \
	"$(INTDIR)\mocha.obj" \
	"$(INTDIR)\mochaapi.obj" \
	"$(INTDIR)\mochalib.obj" \
	"$(INTDIR)\prefapi.obj" \
	"$(INTDIR)\prefdll.res" \
	"..\..\..\..\dist\WIN32_D.OBJ\lib\pr32$(VERSION_NUMBER).lib"

"$(OUTDIR)\libpref.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libpref - Win32 Debug"

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

ALL : "y:\ns\dist\win32_d.obj\bin\nspref.dll"

CLEAN : 
	-@erase "$(INTDIR)\alloca.obj"
	-@erase "$(INTDIR)\mo_array.obj"
	-@erase "$(INTDIR)\mo_atom.obj"
	-@erase "$(INTDIR)\mo_bcode.obj"
	-@erase "$(INTDIR)\mo_bool.obj"
	-@erase "$(INTDIR)\mo_cntxt.obj"
	-@erase "$(INTDIR)\mo_date.obj"
	-@erase "$(INTDIR)\mo_debug.obj"
	-@erase "$(INTDIR)\mo_emit.obj"
	-@erase "$(INTDIR)\mo_fun.obj"
	-@erase "$(INTDIR)\mo_java.obj"
	-@erase "$(INTDIR)\mo_link.obj"
	-@erase "$(INTDIR)\mo_math.obj"
	-@erase "$(INTDIR)\mo_num.obj"
	-@erase "$(INTDIR)\mo_obj.obj"
	-@erase "$(INTDIR)\mo_parse.obj"
	-@erase "$(INTDIR)\mo_scan.obj"
	-@erase "$(INTDIR)\mo_scope.obj"
	-@erase "$(INTDIR)\mo_str.obj"
	-@erase "$(INTDIR)\mocha.obj"
	-@erase "$(INTDIR)\mochaapi.obj"
	-@erase "$(INTDIR)\mochalib.obj"
	-@erase "$(INTDIR)\prefapi.obj"
	-@erase "$(INTDIR)\prefdll.res"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\nspref.exp"
	-@erase "$(OUTDIR)\nspref.lib"
	-@erase "$(OUTDIR)\nspref.pdb"
	-@erase "y:\ns\dist\win32_d.obj\bin\nspref.dll"
	-@erase "y:\ns\dist\win32_d.obj\bin\nspref.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "y:\ns\include" /I "y:\ns\mocha\include" /I "y:\ns\nspr\include" /I "y:\ns\dist\win32_d.obj\include\nspr" /I "y:\ns\sun-java\include" /I "y:\ns\sun-java\md-include" /I "y:\ns\cmd\winfe" /I "y:\ns\modules\libpref\src" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "XP_PC" /YX /c
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "y:\ns\include" /I\
 "y:\ns\mocha\include" /I "y:\ns\nspr\include" /I\
 "y:\ns\dist\win32_d.obj\include\nspr" /I "y:\ns\sun-java\include" /I\
 "y:\ns\sun-java\md-include" /I "y:\ns\cmd\winfe" /I "y:\ns\modules\libpref\src"\
 /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "XP_PC" /Fp"$(INTDIR)/libpref.pch" /YX\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/prefdll.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/libpref.bsc" 
BSC32_SBRS= \

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"y:/ns/dist/win32_d.obj/bin/nspref.dll"
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/nspref.pdb" /debug /machine:I386 /def:".\prefdll.def"\
 /out:"y:/ns/dist/win32_d.obj/bin/nspref.dll" /implib:"$(OUTDIR)/nspref.lib" 
DEF_FILE= \
	".\prefdll.def"
LINK32_OBJS= \
	"$(INTDIR)\alloca.obj" \
	"$(INTDIR)\mo_array.obj" \
	"$(INTDIR)\mo_atom.obj" \
	"$(INTDIR)\mo_bcode.obj" \
	"$(INTDIR)\mo_bool.obj" \
	"$(INTDIR)\mo_cntxt.obj" \
	"$(INTDIR)\mo_date.obj" \
	"$(INTDIR)\mo_debug.obj" \
	"$(INTDIR)\mo_emit.obj" \
	"$(INTDIR)\mo_fun.obj" \
	"$(INTDIR)\mo_java.obj" \
	"$(INTDIR)\mo_link.obj" \
	"$(INTDIR)\mo_math.obj" \
	"$(INTDIR)\mo_num.obj" \
	"$(INTDIR)\mo_obj.obj" \
	"$(INTDIR)\mo_parse.obj" \
	"$(INTDIR)\mo_scan.obj" \
	"$(INTDIR)\mo_scope.obj" \
	"$(INTDIR)\mo_str.obj" \
	"$(INTDIR)\mocha.obj" \
	"$(INTDIR)\mochaapi.obj" \
	"$(INTDIR)\mochalib.obj" \
	"$(INTDIR)\prefapi.obj" \
	"$(INTDIR)\prefdll.res" \
	"..\..\..\..\dist\WIN32_D.OBJ\lib\pr32$(VERSION_NUMBER).lib"

"y:\ns\dist\win32_d.obj\bin\nspref.dll" : "$(OUTDIR)" $(DEF_FILE)\
 $(LINK32_OBJS)
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

# Name "libpref - Win32 Release"
# Name "libpref - Win32 Debug"

!IF  "$(CFG)" == "libpref - Win32 Release"

!ELSEIF  "$(CFG)" == "libpref - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\ns\modules\libpref\src\prefapi.c

!IF  "$(CFG)" == "libpref - Win32 Release"

DEP_CPP_PREFA=\
	"..\prefapi.h"\
	"y:\ns\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"y:\ns\include\winfile.h"\
	"y:\ns\include\xp_core.h"\
	"y:\ns\include\xp_debug.h"\
	"y:\ns\include\xp_file.h"\
	"y:\ns\include\xp_list.h"\
	"y:\ns\include\xp_mcom.h"\
	"y:\ns\include\xp_mem.h"\
	"y:\ns\include\xp_str.h"\
	"y:\ns\include\xp_trace.h"\
	"y:\ns\include\xpassert.h"\
	"y:\ns\mocha\include\mo_pubtd.h"\
	"y:\ns\mocha\include\mochaapi.h"\
	"y:\ns\mocha\include\mochalib.h"\
	"y:\ns\nspr\include\prarena.h"\
	"y:\ns\nspr\include\prmacros.h"\
	"y:\ns\nspr\include\prprf.h"\
	"y:\ns\nspr\include\prtypes.h"\
	"y:\ns\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\

NODEP_CPP_PREFA=\
	"y:\ns\include\macmem.h"\


"$(INTDIR)\prefapi.obj" : $(SOURCE) $(DEP_CPP_PREFA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libpref - Win32 Debug"

DEP_CPP_PREFA=\
	"..\prefapi.h"\
	"y:\ns\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"y:\ns\include\winfile.h"\
	"y:\ns\include\xp_core.h"\
	"y:\ns\include\xp_debug.h"\
	"y:\ns\include\xp_file.h"\
	"y:\ns\include\xp_list.h"\
	"y:\ns\include\xp_mcom.h"\
	"y:\ns\include\xp_mem.h"\
	"y:\ns\include\xp_str.h"\
	"y:\ns\include\xp_trace.h"\
	"y:\ns\include\xpassert.h"\
	"y:\ns\mocha\include\mo_pubtd.h"\
	"y:\ns\mocha\include\mochaapi.h"\
	"y:\ns\mocha\include\mochalib.h"\
	"y:\ns\nspr\include\prarena.h"\
	"y:\ns\nspr\include\prmacros.h"\
	"y:\ns\nspr\include\prprf.h"\
	"y:\ns\nspr\include\prtypes.h"\
	"y:\ns\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\

NODEP_CPP_PREFA=\
	"y:\ns\include\macmem.h"\


"$(INTDIR)\prefapi.obj" : $(SOURCE) $(DEP_CPP_PREFA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mochalib.c
DEP_CPP_MOCHA=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mochalib.obj" : $(SOURCE) $(DEP_CPP_MOCHA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mochaapi.c
DEP_CPP_MOCHAA=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_parse.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scan.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mochaapi.obj" : $(SOURCE) $(DEP_CPP_MOCHAA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mocha.c
DEP_CPP_MOCHA_=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\mocha\src\alloca.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mocha.obj" : $(SOURCE) $(DEP_CPP_MOCHA_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_str.c
DEP_CPP_MO_ST=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\mocha\src\alloca.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_str.obj" : $(SOURCE) $(DEP_CPP_MO_ST) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_scope.c
DEP_CPP_MO_SC=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_scope.obj" : $(SOURCE) $(DEP_CPP_MO_SC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_scan.c
DEP_CPP_MO_SCA=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scan.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_scan.obj" : $(SOURCE) $(DEP_CPP_MO_SCA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_parse.c
DEP_CPP_MO_PA=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_parse.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scan.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_parse.obj" : $(SOURCE) $(DEP_CPP_MO_PA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_obj.c
DEP_CPP_MO_OB=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\mocha\src\alloca.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_obj.obj" : $(SOURCE) $(DEP_CPP_MO_OB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_num.c
DEP_CPP_MO_NU=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prdtoa.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_num.obj" : $(SOURCE) $(DEP_CPP_MO_NU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_math.c
DEP_CPP_MO_MA=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlong.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtime.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_math.obj" : $(SOURCE) $(DEP_CPP_MO_MA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_link.c

"$(INTDIR)\mo_link.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_java.c
DEP_CPP_MO_JA=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\include\cdefs.h"\
	"..\..\..\..\include\edttypes.h"\
	"..\..\..\..\include\jri.h"\
	"..\..\..\..\include\jri_md.h"\
	"..\..\..\..\include\jriext.h"\
	"..\..\..\..\include\jritypes.h"\
	"..\..\..\..\include\mcom_db.h"\
	"..\..\..\..\include\minicom.h"\
	"..\..\..\..\include\ntypes.h"\
	"..\..\..\..\include\ssl.h"\
	"..\..\..\..\include\winfile.h"\
	"..\..\..\..\include\xp_core.h"\
	"..\..\..\..\include\xp_debug.h"\
	"..\..\..\..\include\xp_error.h"\
	"..\..\..\..\include\xp_file.h"\
	"..\..\..\..\include\xp_list.h"\
	"..\..\..\..\include\xp_mcom.h"\
	"..\..\..\..\include\xp_mem.h"\
	"..\..\..\..\include\xp_sock.h"\
	"..\..\..\..\include\xp_str.h"\
	"..\..\..\..\include\xp_trace.h"\
	"..\..\..\..\include\xpassert.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_java.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prfile.h"\
	"..\..\..\..\nspr\include\prgc.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prlong.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prmon.h"\
	"..\..\..\..\nspr\include\prntohl.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prthread.h"\
	"..\..\..\..\nspr\include\prtime.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	"..\..\..\..\sun-java\include\config.h"\
	"..\..\..\..\sun-java\include\debug.h"\
	"..\..\..\..\sun-java\include\exceptions.h"\
	"..\..\..\..\sun-java\include\interpreter.h"\
	"..\..\..\..\sun-java\include\javaString.h"\
	"..\..\..\..\sun-java\include\javaThreads.h"\
	"..\..\..\..\sun-java\include\monitor.h"\
	"..\..\..\..\sun-java\include\oobj.h"\
	"..\..\..\..\sun-java\include\signature.h"\
	"..\..\..\..\sun-java\include\standardlib.h"\
	"..\..\..\..\sun-java\include\sys_api.h"\
	"..\..\..\..\sun-java\include\timeval.h"\
	"..\..\..\..\sun-java\include\typedefs.h"\
	"..\..\..\..\sun-java\md-include\nspr_md.h"\
	"..\..\..\..\sun-java\md-include\oobj_md.h"\
	"..\..\..\..\sun-java\md-include\sysmacros_md.h"\
	"..\..\..\..\sun-java\md-include\timeval_md.h"\
	"..\..\..\..\sun-java\md-include\typedefs_md.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\

NODEP_CPP_MO_JA=\
	"..\..\..\..\include\macmem.h"\
	"..\..\..\..\include\macsock.h"\
	"..\..\..\..\mocha\src\java_applet_Applet.h"\
	"..\..\..\..\mocha\src\java_lang_ClassLoader.h"\
	"..\..\..\..\mocha\src\java_lang_Throwable.h"\
	"..\..\..\..\mocha\src\lj.h"\
	"..\..\..\..\mocha\src\n_javascript_JSException.h"\
	"..\..\..\..\mocha\src\netscape_javascript_JSException.h"\
	"..\..\..\..\mocha\src\netscape_javascript_JSObject.h"\
	"..\..\..\..\mocha\src\opcodes.h"\
	"..\..\..\..\nspr\include\macsock.h"\
	"..\..\..\..\sun-java\include\java_lang_String.h"\
	"..\..\..\..\sun-java\include\java_lang_Thread.h"\
	"..\..\..\..\sun-java\include\java_lang_ThreadGroup.h"\
	"..\..\..\..\sun-java\include\jdk_java_lang_String.h"\


"$(INTDIR)\mo_java.obj" : $(SOURCE) $(DEP_CPP_MO_JA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_fun.c
DEP_CPP_MO_FU=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_parse.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scan.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_fun.obj" : $(SOURCE) $(DEP_CPP_MO_FU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_emit.c
DEP_CPP_MO_EM=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_emit.obj" : $(SOURCE) $(DEP_CPP_MO_EM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_debug.c

"$(INTDIR)\mo_debug.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_date.c
DEP_CPP_MO_DA=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlong.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmjtime.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtime.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_date.obj" : $(SOURCE) $(DEP_CPP_MO_DA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_cntxt.c
DEP_CPP_MO_CN=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scan.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_cntxt.obj" : $(SOURCE) $(DEP_CPP_MO_CN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_bool.c
DEP_CPP_MO_BO=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_bool.obj" : $(SOURCE) $(DEP_CPP_MO_BO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_bcode.c
DEP_CPP_MO_BC=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\src\alloca.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_bcode.obj" : $(SOURCE) $(DEP_CPP_MO_BC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_atom.c
DEP_CPP_MO_AT=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_bcode.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_emit.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.def"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prglobal.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prlog.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_atom.obj" : $(SOURCE) $(DEP_CPP_MO_AT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\mo_array.c
DEP_CPP_MO_AR=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\mocha\include\mo_atom.h"\
	"..\..\..\..\mocha\include\mo_cntxt.h"\
	"..\..\..\..\mocha\include\mo_prvtd.h"\
	"..\..\..\..\mocha\include\mo_pubtd.h"\
	"..\..\..\..\mocha\include\mo_scope.h"\
	"..\..\..\..\mocha\include\mocha.h"\
	"..\..\..\..\mocha\include\mochaapi.h"\
	"..\..\..\..\mocha\include\mochalib.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prclist.h"\
	"..\..\..\..\nspr\include\prhash.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\


"$(INTDIR)\mo_array.obj" : $(SOURCE) $(DEP_CPP_MO_AR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\mocha\src\alloca.c
DEP_CPP_ALLOC=\
	"..\..\..\..\dist\win32_d.obj\include\nspr\prcpucfg.h"\
	"..\..\..\..\include\cdefs.h"\
	"..\..\..\..\include\mcom_db.h"\
	"..\..\..\..\include\winfile.h"\
	"..\..\..\..\include\xp_core.h"\
	"..\..\..\..\include\xp_debug.h"\
	"..\..\..\..\include\xp_file.h"\
	"..\..\..\..\include\xp_list.h"\
	"..\..\..\..\include\xp_mcom.h"\
	"..\..\..\..\include\xp_mem.h"\
	"..\..\..\..\include\xp_str.h"\
	"..\..\..\..\include\xp_trace.h"\
	"..\..\..\..\include\xpassert.h"\
	"..\..\..\..\nspr\include\os\aix.h"\
	"..\..\..\..\nspr\include\os\bsdi.h"\
	"..\..\..\..\nspr\include\os\hpux.h"\
	"..\..\..\..\nspr\include\os\irix.h"\
	"..\..\..\..\nspr\include\os\linux.h"\
	"..\..\..\..\nspr\include\os\nec.h"\
	"..\..\..\..\nspr\include\os\osf1.h"\
	"..\..\..\..\nspr\include\os\reliantunix.h"\
	"..\..\..\..\nspr\include\os\scoos.h"\
	"..\..\..\..\nspr\include\os\solaris.h"\
	"..\..\..\..\nspr\include\os\sony.h"\
	"..\..\..\..\nspr\include\os\sunos.h"\
	"..\..\..\..\nspr\include\os\unixware.h"\
	"..\..\..\..\nspr\include\os\win16.h"\
	"..\..\..\..\nspr\include\os\win32.h"\
	"..\..\..\..\nspr\include\prarena.h"\
	"..\..\..\..\nspr\include\prmacos.h"\
	"..\..\..\..\nspr\include\prmacros.h"\
	"..\..\..\..\nspr\include\prmem.h"\
	"..\..\..\..\nspr\include\prosdep.h"\
	"..\..\..\..\nspr\include\prpcos.h"\
	"..\..\..\..\nspr\include\prprf.h"\
	"..\..\..\..\nspr\include\prsystem.h"\
	"..\..\..\..\nspr\include\prtypes.h"\
	"..\..\..\..\nspr\include\prunixos.h"\
	"..\..\..\..\nspr\include\sunos4.h"\
	"..\..\..\..\sun-java\include\config.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\

NODEP_CPP_ALLOC=\
	"..\..\..\..\include\macmem.h"\


"$(INTDIR)\alloca.obj" : $(SOURCE) $(DEP_CPP_ALLOC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prefdll.def

!IF  "$(CFG)" == "libpref - Win32 Release"

!ELSEIF  "$(CFG)" == "libpref - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\res\prefdll.rc
DEP_RSC_PREFD=\
	".\res\init.rc"\


!IF  "$(CFG)" == "libpref - Win32 Release"


"$(INTDIR)\prefdll.res" : $(SOURCE) $(DEP_RSC_PREFD) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/prefdll.res" /i "res" /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "libpref - Win32 Debug"


"$(INTDIR)\prefdll.res" : $(SOURCE) $(DEP_RSC_PREFD) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/prefdll.res" /i "res" /d "_DEBUG" $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\dist\WIN32_D.OBJ\lib\pr32$(VERSION_NUMBER).lib

!IF  "$(CFG)" == "libpref - Win32 Release"

!ELSEIF  "$(CFG)" == "libpref - Win32 Debug"

!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
