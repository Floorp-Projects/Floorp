# Following taken from nsdefs.mk PY. 
# nsarch renamed to getarch PY.

TMP_ARCH := $(shell uname -s)
ifeq ($(TMP_ARCH), WINNT)
BUILD_ARCH := $(shell uname -s)
else
BUILD_ARCH := $(shell $(CONFIG_ROOT)/getarch)
endif

BUILD_DEBUG=full
BUILD_SECURITY=export

ARCH=$(BUILD_ARCH)
SECURITY=$(BUILD_SECURITY)
DEBUG=$(BUILD_DEBUG)

PRODUCT="Netscape Messaging SDK"
DIR=sdk
COMMON_OBJDIR=$(MSGSDK_ROOT)/built/$(ARCH)-$(SECURITY)-$(DEBUG)-$(DIR)
OBJDIR=$(COMMON_OBJDIR)
NS_BUILD_FLAVOR=$(ARCH)-$(SECURITY)-$(DEBUG)-$(DIR)
NS_RELEASE=$(MSGSDK_ROOT)/built/release/$(ARCH)-$(SECURITY)-$(DEBUG)-$(DIR)
DO_SEARCH=yes

# Following taken from nsconfig.mk
#MAKE=gmake $(BUILDOPT) NO_MOCHA=1
all:

MAKE=gmake 

NSOS_ARCH := $(subst /,_,$(shell uname -s))

ifeq ($(NSOS_ARCH), IRIX64)
NSOS_ARCH := IRIX
endif

ifeq ($(NSOS_ARCH), AIX)
 NSOS_TEST        := $(shell uname -v)
 ifeq ($(NSOS_TEST),3)
   NSOS_RELEASE   := $(shell uname -r)
 else
   NSOS_RELEASE   := $(shell uname -v)_$(shell uname -r)
 endif
else
 NSOS_RELEASE      := $(shell uname -r)
endif

ifeq ($(ARCH), SOLARIS)
OSVERSION := $(shell uname -r | sed "y/./0/")
endif

NSOS_TEST1       := $(shell uname -m)
ifeq ($(NSOS_TEST1),i86pc)
 NSCONFIG         = $(NSOS_ARCH)$(NSOS_RELEASE)_$(NSOS_TEST1)
else
 NSCONFIG         = $(NSOS_ARCH)$(NSOS_RELEASE)
endif


# ---------------------- OS-specific compile flags -----------------------


ifeq ($(ARCH), AIX)
ifeq ($(NSCONFIG), AIX4_2)
CC=cc -DAIXV3 -DAIXV4 -D_AIX32_CURSES 
CPPCMD=/usr/ccs/lib/cpp -P
ARCH_DEBUG=-g
ARCH_OPT=-g -O2
RANLIB=ranlib
NONSHARED_FLAG=-bnso -bI:/lib/syscalls.exp
#
DLL_LDFLAGS=-G -bM:SRE
EXTRA_LIBS=-brtl -ldl -lm -lc -bI:/usr/lib/lowsys.exp
endif
endif

ifeq ($(ARCH), HPUX)
CC=cc
ARCH_DEBUG=-g
ARCH_OPT=-O
# Compile everything pos-independent
ARCH_CFLAGS=-D_HPUX_SOURCE -Aa +DA1.0 +z
RANLIB=true
NONSHARED_FLAG=-Wl,-a,archive
#
EXTRA_LIBS=-Wl,-E -ldld -lm
DLL_CFLAGS=+z
DLL_LDFLAGS=-b
NSAPI_CAPABLE=true
VERITY_LIB=_hp800
endif

ifeq ($(NSOS_ARCH), IRIX)
CC=cc -DSVR4
CXX=CC -DSVR4
CCC=$(CXX)
ARCH_DEBUG=-g
ARCH_OPT=-O
ARCH_CFLAGS=-fullwarn -use_readonly_const -MDupdate .depends
RANLIB=true
DLL_LDFLAGS=-shared -all
NONSHARED_FLAG=-non_shared
NLIST=-lmld
NSAPI_CAPABLE=true
VERITY_LIB=_mipsabi
endif

ifeq ($(ARCH), OSF1)
ifndef CC
CC=cc
endif
CC += -taso
ARCH_DEBUG=-g
ARCH_OPT=-O2
ARCH_CFLAGS=-DIS_64 -ieee_with_inexact
RANLIB=ranlib
DLL_LDFLAGS=-shared -all -expect_unresolved "*" -taso
NONSHARED_FLAG=-non_shared
NSAPI_CAPABLE=true
VERITY_LIB=_asof32
endif

ifeq ($(ARCH), SOLARIS)
CC=cc
CXX=CC
CCC=$(CXX)
ARCH_OPT=-xO2
ARCH_CFLAGS=-DSVR4 -D__svr4 -D__svr4__ -D_SVID_GETTOD -DOSVERSION=$(OSVERSION)
ARCH_DEBUG=-g
ARCH_OPT=-xO2
RANLIB=true
EXTRA_LIBS += -lsocket -lnsl -ldl -lresolv
DLL_LDFLAGS=-G
NONSHARED_FLAG=-static
NSAPI_CAPABLE=true
NLIST=-lelf
VERITY_LIB=_solaris
endif

ifeq ($(ARCH), WINNT)
PROCESSOR := $(shell uname -p)
ifeq ($(PROCESSOR), I386)
CPU_ARCH = x386
CC=cl -nologo -MD -W3 -GX -D_X86_ -Dx386 -DWIN32 -D_WINDOWS
CCP=cl -nologo -MD -W3 -GX -D_X86_ -Dx386 -DWIN32 -D_WINDOWS -D_MBCS -D_AFXDLL
else 
CPU_ARCH = processor_is_undefined
endif
RC=rc $(MCC_SERVER)
MC=mc
ARCH_DEBUG=-D_DEBUG -Od	-Z7
ARCH_OPT=-DNDEBUG -O2
ARCH_CFLAGS=
ARCH_LINK_DEBUG=-DEBUG
ARCH_LINK_OPT=
RANLIB=echo

EXTRA_LIBS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib \
           comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib \
           rpcrt4.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib

DLL_LDFLAGS=
NONSHARED_FLAG=
NSAPI_CAPABLE=true
VERITY_LIB=_nti152
endif

ifeq ($(ARCH), SUNOS4)
MATHLIB=/usr/local/sun4/lib/libmgnuPIC.a
else
MATHLIB=-lm
endif

# ------------------------ The actual build rules ------------------------
RM=rm
DEPEND=makedepend
STRIP=strip

ifeq ($(ARCH), WINNT)
XP_FLAG=-DXP_WIN32  -DXP_WIN -D_WINDOWS -DXP_PC -DXP_WINNT
else
XP_FLAG=-DXP_UNIX
endif

CFLAGS_NO_ARCH=$(XP_FLAG) -D$(ARCH) 
CFLAGS=$(ARCH_CFLAGS) $(CFLAGS_NO_ARCH) $(ARCH_OPT)


ifndef NOSTDCOMPILE
$(OBJDEST)/%.o: %.c
ifeq ($(ARCH), WINNT)
ifeq ($(BOUNDS_CHECKER), yes)
	bcompile -c -Zop $(NSROOT)/bchecker.ini -nologo -MD -W3 -GX -DWIN32 \
	    -D_WINDOWS $(CFLAGS) $(MCC_INCLUDE) $< -Fo$(OBJDEST)/$*.o
else
	$(CC) -c $(CFLAGS) $(MCC_INCLUDE) $< -Fo$(OBJDEST)/$*.o $(CBSCFLAGS)
endif
else
	$(CC) -c $(CFLAGS) $(MCC_INCLUDE) $< -o $(OBJDEST)/$*.o
endif
endif

ifeq ($(ARCH), WINNT)
LIB_SUFFIX=lib
DLL_SUFFIX=dll
AR=lib /nologo /out:"$@"
LINK_DLL	= link /nologo /MAP /DLL /PDB:NONE $(ML_DEBUG) /SUBSYSTEM:WINDOWS $(LLFLAGS) $(DLL_LDFLAGS) /out:"$@"
LINK_EXE	= link -OUT:"$@" /MAP  $(ARCH_LINK_DEBUG) $(LCFLAGS) /NOLOGO /PDB:NONE /INCREMENTAL:NO \
                  /SUBSYSTEM:windows $(OBJS) $(DEPLIBS) $(EXTRA_LIBS)
BSCMAKE = bscmake.exe /nologo /o $@
$(OBJDEST)/%.res: %.rc
	$(RC) -Fo$@ $*.rc
else
ifeq ($(ARCH), HPUX)
DLL_SUFFIX=sl
LIB_SUFFIX=a
AR=ar rcv $@
LINK_DLL=ld $(DLL_LDFLAGS) -o $@
else
LIB_SUFFIX=a
DLL_SUFFIX=so
AR=ar rcv $@
LINK_DLL=ld $(DLL_LDFLAGS) -o $@
endif
endif
ifndef NOSTDCLEAN
clean:
	$(RM) -f .depends $(LIBS) $(OBJDEST)/*.o *_pure_* $(BINS) $(PUREFILES)
endif

ifndef NOSTDDEPEND
ifeq ($(ARCH), WINNT)
INCLUDE_DEPENDS = $(NULL)
depend:
	echo making depends
else
INCLUDE_DEPENDS = .depends
.depends:
	touch .depends

depend:
	$(DEPEND) -f.depends -- $(MCC_INCLUDE) $(CFLAGS) *.c *.cpp
endif
endif

ifndef NOSTDSTRIP
strip:
	$(STRIP) $(BINS)
endif
