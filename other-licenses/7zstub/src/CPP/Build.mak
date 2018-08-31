LIBS = $(LIBS) oleaut32.lib ole32.lib

!IFNDEF MY_NO_UNICODE
CFLAGS = $(CFLAGS) -DUNICODE -D_UNICODE
!ENDIF

# CFLAGS = $(CFLAGS) -FAsc -Fa$O/Asm/

!IFNDEF O
!IFDEF CPU
O=$(CPU)
!ELSE
O=O
!ENDIF
!ENDIF

!IF "$(CPU)" == "AMD64"
MY_ML = ml64 -Dx64 -WX
!ELSEIF "$(CPU)" == "ARM"
MY_ML = armasm -WX
!ELSE
MY_ML = ml -WX
!ENDIF


!IFDEF UNDER_CE
RFLAGS = $(RFLAGS) -dUNDER_CE
!IFDEF MY_CONSOLE
LFLAGS = $(LFLAGS) /ENTRY:mainACRTStartup
!ENDIF
!ELSE
!IFNDEF NEW_COMPILER
LFLAGS = $(LFLAGS) -OPT:NOWIN98
!ENDIF
!IF "$(CPU)" != "ARM" && "$(CPU)" != "ARM64"
CFLAGS = $(CFLAGS) -Gr
!ENDIF
LIBS = $(LIBS) user32.lib advapi32.lib shell32.lib
!ENDIF

!IF "$(CPU)" == "ARM"
COMPL_ASM = $(MY_ML) $** $O/$(*B).obj
!ELSE
COMPL_ASM = $(MY_ML) -c -Fo$O/ $**
!ENDIF

CFLAGS = $(CFLAGS) -nologo -c -Fo$O/ -W4 -WX -EHsc -Gy -GR- -GF

!IFDEF MY_STATIC_LINK
!IFNDEF MY_SINGLE_THREAD
CFLAGS = $(CFLAGS) -MT
!ENDIF
!ELSE
CFLAGS = $(CFLAGS) -MD
!ENDIF

!IFDEF NEW_COMPILER
CFLAGS = $(CFLAGS) -GS- -Zc:forScope -Zc:wchar_t
!IFNDEF UNDER_CE
CFLAGS = $(CFLAGS) -MP2
!IFNDEF CPU
# CFLAGS = $(CFLAGS) -arch:IA32
!ENDIF
!ENDIF
!ELSE
CFLAGS = $(CFLAGS)
!ENDIF

!IF "$(CPU)" == "AMD64"
CFLAGS_O1 = $(CFLAGS) -O1
!ELSE
CFLAGS_O1 = $(CFLAGS) -O1
!ENDIF
CFLAGS_O2 = $(CFLAGS) -O2

LFLAGS = $(LFLAGS) -nologo -OPT:REF -OPT:ICF

!IFNDEF UNDER_CE
LFLAGS = $(LFLAGS) /LARGEADDRESSAWARE
!ENDIF

!IFDEF DEF_FILE
LFLAGS = $(LFLAGS) -DLL -DEF:$(DEF_FILE)
!ELSE
!IF defined(MY_FIXED) && "$(CPU)" != "ARM" && "$(CPU)" != "ARM64"
LFLAGS = $(LFLAGS) /FIXED
!ELSE
LFLAGS = $(LFLAGS) /FIXED:NO
!ENDIF
# /BASE:0x400000
!ENDIF


# !IF "$(CPU)" == "AMD64"

!IFDEF SUB_SYS_VER

MY_SUB_SYS_VER=5.02

!IFDEF MY_CONSOLE
LFLAGS = $(LFLAGS) /SUBSYSTEM:console,$(MY_SUB_SYS_VER)
!ELSE
LFLAGS = $(LFLAGS) /SUBSYSTEM:windows,$(MY_SUB_SYS_VER)
!ENDIF

!ENDIF


PROGPATH = $O\$(PROG)

COMPL_O1   = $(CC) $(CFLAGS_O1) $**
COMPL_O2   = $(CC) $(CFLAGS_O2) $**
COMPL_PCH  = $(CC) $(CFLAGS_O1) -Yc"StdAfx.h" -Fp$O/a.pch $**
COMPL      = $(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $**

COMPLB    = $(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $<
# COMPLB_O2 = $(CC) $(CFLAGS_O2) -Yu"StdAfx.h" -Fp$O/a.pch $<
COMPLB_O2 = $(CC) $(CFLAGS_O2) $<

CFLAGS_C_ALL = $(CFLAGS_O2) $(CFLAGS_C_SPEC)
CCOMPL_PCH  = $(CC) $(CFLAGS_C_ALL) -Yc"Precomp.h" -Fp$O/a.pch $**
CCOMPL_USE  = $(CC) $(CFLAGS_C_ALL) -Yu"Precomp.h" -Fp$O/a.pch $**
CCOMPL      = $(CC) $(CFLAGS_C_ALL) $**
CCOMPLB     = $(CC) $(CFLAGS_C_ALL) $<


all: $(PROGPATH)

clean:
	-del /Q $(PROGPATH) $O\*.exe $O\*.dll $O\*.obj $O\*.lib $O\*.exp $O\*.res $O\*.pch $O\*.asm

$O:
	if not exist "$O" mkdir "$O"
$O/Asm:
	if not exist "$O/Asm" mkdir "$O/Asm"

$(PROGPATH): $O $O/Asm $(OBJS) $(DEF_FILE)
	link $(LFLAGS) -out:$(PROGPATH) $(OBJS) $(LIBS)

!IFNDEF NO_DEFAULT_RES
$O\resource.res: $(*B).rc
	rc $(RFLAGS) -fo$@ $**
!ENDIF
$O\StdAfx.obj: $(*B).cpp
	$(COMPL_PCH)
