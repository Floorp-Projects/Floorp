# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=cb - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to cb - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "cb - Win32 Release" && "$(CFG)" != "cb - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "cb.mak" CFG="cb - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cb - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "cb - Win32 Debug" (based on "Win32 (x86) Application")
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
# PROP Target_Last_Scanned "cb - Win32 Debug"
CPP=cl.exe
RSC=rc.exe
MTL=mktyplib.exe

!IF  "$(CFG)" == "cb - Win32 Release"

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
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\cb.exe"

CLEAN : 
	-@erase "$(INTDIR)\CacheTreeView.obj"
	-@erase "$(INTDIR)\cb.obj"
	-@erase "$(INTDIR)\cb.pch"
	-@erase "$(INTDIR)\cb.res"
	-@erase "$(INTDIR)\cbDoc.obj"
	-@erase "$(INTDIR)\cbView.obj"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(OUTDIR)\cb.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\..\..\..\dist\public\cache" /I "..\..\..\..\..\dist\win32_d.obj\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "..\..\..\..\..\dist\public\cache" /I\
 "..\..\..\..\..\dist\win32_d.obj\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/cb.pch" /Yu"stdafx.h" /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/cb.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/cb.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=/nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)/cb.pdb"\
 /machine:I386 /out:"$(OUTDIR)/cb.exe" 
LINK32_OBJS= \
	"$(INTDIR)\CacheTreeView.obj" \
	"$(INTDIR)\cb.obj" \
	"$(INTDIR)\cb.res" \
	"$(INTDIR)\cbDoc.obj" \
	"$(INTDIR)\cbView.obj" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\StdAfx.obj"

"$(OUTDIR)\cb.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "cb - Win32 Debug"

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
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\cb.exe" "$(OUTDIR)\cb.bsc"

CLEAN : 
	-@erase "$(INTDIR)\CacheTreeView.obj"
	-@erase "$(INTDIR)\CacheTreeView.sbr"
	-@erase "$(INTDIR)\cb.obj"
	-@erase "$(INTDIR)\cb.pch"
	-@erase "$(INTDIR)\cb.res"
	-@erase "$(INTDIR)\cb.sbr"
	-@erase "$(INTDIR)\cbDoc.obj"
	-@erase "$(INTDIR)\cbDoc.sbr"
	-@erase "$(INTDIR)\cbView.obj"
	-@erase "$(INTDIR)\cbView.sbr"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\MainFrm.sbr"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\StdAfx.sbr"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\cb.bsc"
	-@erase "$(OUTDIR)\cb.exe"
	-@erase "$(OUTDIR)\cb.ilk"
	-@erase "$(OUTDIR)\cb.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\..\dist\public\cache" /I "..\..\..\..\..\dist\win32_d.obj\include" /I "..\..\..\..\..\dist\public\dbm" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /Yu"stdafx.h" /V"ERBOSE:lib" /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\..\dist\public\cache"\
 /I "..\..\..\..\..\dist\win32_d.obj\include" /I\
 "..\..\..\..\..\dist\public\dbm" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/cb.pch" /Yu"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /V"ERBOSE:lib" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/cb.res" /d "_DEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/cb.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\CacheTreeView.sbr" \
	"$(INTDIR)\cb.sbr" \
	"$(INTDIR)\cbDoc.sbr" \
	"$(INTDIR)\cbView.sbr" \
	"$(INTDIR)\MainFrm.sbr" \
	"$(INTDIR)\StdAfx.sbr"

"$(OUTDIR)\cb.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 ..\..\..\..\..\dist\win32_d.obj\lib\nspr3.lib ..\..\..\..\..\dist\win32_d.obj\lib\plc3.lib ..\..\..\..\..\dist\win32_d.obj\lib\dbm32.lib ..\..\..\..\..\dist\win32_d.obj\lib\cachelib.lib /nologo /subsystem:windows /debug /machine:I386
# SUBTRACT LINK32 /nodefaultlib
LINK32_FLAGS=..\..\..\..\..\dist\win32_d.obj\lib\nspr3.lib\
 ..\..\..\..\..\dist\win32_d.obj\lib\plc3.lib\
 ..\..\..\..\..\dist\win32_d.obj\lib\dbm32.lib\
 ..\..\..\..\..\dist\win32_d.obj\lib\cachelib.lib /nologo /subsystem:windows\
 /incremental:yes /pdb:"$(OUTDIR)/cb.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/cb.exe" 
LINK32_OBJS= \
	"$(INTDIR)\CacheTreeView.obj" \
	"$(INTDIR)\cb.obj" \
	"$(INTDIR)\cb.res" \
	"$(INTDIR)\cbDoc.obj" \
	"$(INTDIR)\cbView.obj" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\StdAfx.obj"

"$(OUTDIR)\cb.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "cb - Win32 Release"
# Name "cb - Win32 Debug"

!IF  "$(CFG)" == "cb - Win32 Release"

!ELSEIF  "$(CFG)" == "cb - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\cb.cpp
DEP_CPP_CB_CP=\
	".\cb.h"\
	".\cbDoc.h"\
	".\cbView.h"\
	".\MainFrm.h"\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "cb - Win32 Release"


"$(INTDIR)\cb.obj" : $(SOURCE) $(DEP_CPP_CB_CP) "$(INTDIR)" "$(INTDIR)\cb.pch"


!ELSEIF  "$(CFG)" == "cb - Win32 Debug"


"$(INTDIR)\cb.obj" : $(SOURCE) $(DEP_CPP_CB_CP) "$(INTDIR)" "$(INTDIR)\cb.pch"

"$(INTDIR)\cb.sbr" : $(SOURCE) $(DEP_CPP_CB_CP) "$(INTDIR)" "$(INTDIR)\cb.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\StdAfx.cpp
DEP_CPP_STDAF=\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "cb - Win32 Release"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MD /W3 /GX /O2 /I "..\..\..\..\..\dist\public\cache" /I\
 "..\..\..\..\..\dist\win32_d.obj\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/cb.pch" /Yc"stdafx.h" /Fo"$(INTDIR)/" /c\
 $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\cb.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ELSEIF  "$(CFG)" == "cb - Win32 Debug"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\..\dist\public\cache"\
 /I "..\..\..\..\..\dist\win32_d.obj\include" /I\
 "..\..\..\..\..\dist\public\dbm" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/cb.pch" /Yc"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /V"ERBOSE:lib" /c $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\StdAfx.sbr" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\cb.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\MainFrm.cpp
DEP_CPP_MAINF=\
	".\CacheTreeView.h"\
	".\cb.h"\
	".\MainFrm.h"\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "cb - Win32 Release"


"$(INTDIR)\MainFrm.obj" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ELSEIF  "$(CFG)" == "cb - Win32 Debug"


"$(INTDIR)\MainFrm.obj" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"

"$(INTDIR)\MainFrm.sbr" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\cbDoc.cpp
DEP_CPP_CBDOC=\
	".\CacheTreeView.h"\
	".\cb.h"\
	".\cbDoc.h"\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "cb - Win32 Release"


"$(INTDIR)\cbDoc.obj" : $(SOURCE) $(DEP_CPP_CBDOC) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ELSEIF  "$(CFG)" == "cb - Win32 Debug"


"$(INTDIR)\cbDoc.obj" : $(SOURCE) $(DEP_CPP_CBDOC) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"

"$(INTDIR)\cbDoc.sbr" : $(SOURCE) $(DEP_CPP_CBDOC) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\cbView.cpp

!IF  "$(CFG)" == "cb - Win32 Release"

DEP_CPP_CBVIE=\
	"..\..\..\..\..\dist\public\cache\nsBkgThread.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheBkgThd.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheManager.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheModule.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheObject.h"\
	"..\..\..\..\..\dist\public\cache\nsCachePref.h"\
	"..\..\..\..\..\dist\public\cache\nsDiskModule.h"\
	"..\..\..\..\..\dist\public\cache\nsEnumeration.h"\
	"..\..\..\..\..\dist\public\cache\nsIterator.h"\
	"..\..\..\..\..\dist\public\cache\nsMemCacheObject.h"\
	"..\..\..\..\..\dist\public\cache\nsMemModule.h"\
	"..\..\..\..\..\dist\public\cache\nsMemStream.h"\
	"..\..\..\..\..\dist\public\cache\nsMonitorable.h"\
	"..\..\..\..\..\dist\public\cache\nsStream.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\obsolete\protypes.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prcpucfg.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinrval.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prlog.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prmon.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prthread.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtypes.h"\
	".\cb.h"\
	".\cbDoc.h"\
	".\cbView.h"\
	".\MainFrm.h"\
	".\nsTimeIt.h"\
	".\StdAfx.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_CBVIE=\
	"..\..\..\..\..\dist\public\cache\mcom_db.h"\
	"..\..\..\..\..\dist\public\cache\nsISupports.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\protypes.h"\
	

"$(INTDIR)\cbView.obj" : $(SOURCE) $(DEP_CPP_CBVIE) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ELSEIF  "$(CFG)" == "cb - Win32 Debug"

DEP_CPP_CBVIE=\
	"..\..\..\..\..\dist\public\cache\nsBkgThread.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheBkgThd.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheManager.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheModule.h"\
	"..\..\..\..\..\dist\public\cache\nsCacheObject.h"\
	"..\..\..\..\..\dist\public\cache\nsCachePref.h"\
	"..\..\..\..\..\dist\public\cache\nsDiskModule.h"\
	"..\..\..\..\..\dist\public\cache\nsEnumeration.h"\
	"..\..\..\..\..\dist\public\cache\nsIterator.h"\
	"..\..\..\..\..\dist\public\cache\nsMemCacheObject.h"\
	"..\..\..\..\..\dist\public\cache\nsMemModule.h"\
	"..\..\..\..\..\dist\public\cache\nsMemStream.h"\
	"..\..\..\..\..\dist\public\cache\nsMonitorable.h"\
	"..\..\..\..\..\dist\public\cache\nsStream.h"\
	"..\..\..\..\..\dist\public\dbm\cdefs.h"\
	"..\..\..\..\..\dist\public\dbm\mcom_db.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\obsolete\protypes.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prcpucfg.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prinrval.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prlog.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prmon.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prthread.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\prtypes.h"\
	".\cb.h"\
	".\cbDoc.h"\
	".\cbView.h"\
	".\MainFrm.h"\
	".\nsTimeIt.h"\
	".\StdAfx.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_CBVIE=\
	"..\..\..\..\..\dist\public\cache\nsISupports.h"\
	"..\..\..\..\..\dist\public\dbm\prmacos.h"\
	"..\..\..\..\..\dist\public\dbm\xp_mcom.h"\
	"..\..\..\..\..\dist\win32_d.obj\include\protypes.h"\
	

"$(INTDIR)\cbView.obj" : $(SOURCE) $(DEP_CPP_CBVIE) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"

"$(INTDIR)\cbView.sbr" : $(SOURCE) $(DEP_CPP_CBVIE) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\cb.rc
DEP_RSC_CB_RC=\
	".\res\cacheico.bmp"\
	".\res\cb.ico"\
	".\res\cb.rc2"\
	".\res\cbDoc.ico"\
	".\res\Toolbar.bmp"\
	

"$(INTDIR)\cb.res" : $(SOURCE) $(DEP_RSC_CB_RC) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\CacheTreeView.cpp
DEP_CPP_CACHE=\
	".\CacheTreeView.h"\
	".\cb.h"\
	".\cbDoc.h"\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "cb - Win32 Release"


"$(INTDIR)\CacheTreeView.obj" : $(SOURCE) $(DEP_CPP_CACHE) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ELSEIF  "$(CFG)" == "cb - Win32 Debug"


"$(INTDIR)\CacheTreeView.obj" : $(SOURCE) $(DEP_CPP_CACHE) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"

"$(INTDIR)\CacheTreeView.sbr" : $(SOURCE) $(DEP_CPP_CACHE) "$(INTDIR)"\
 "$(INTDIR)\cb.pch"


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
