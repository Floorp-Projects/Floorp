# Figure out the OS sepecific setup stuff

# Define a flag for include-at-most-once
INCLUDED_CONFIG_MK = 1

# These normally get overridden on the command line from ../Makefile
# This is the default build type
BUILD_VARIANT	= release
ifeq ($(BUILD_VARIANT), debug)
	BUILD_TYPE = DEBUG
	OBJDIR_TAG = _DBG
else
	BUILD_TYPE = RELEASE
	OBJDIR_TAG = _OPT
endif

OS_ARCH := $(shell uname -s)

ifeq ("$(OBJDIR)", "")
ifeq ($(OS_ARCH), WINNT)
	NSARCH := WIN32
else
	NSARCH := $(shell $(topsrcdir)/nsarch)
endif
	OBJDIR = $(topsrcdir)/build/$(NSARCH)$(OBJDIR_TAG).OBJ
	PKGDIR = $(topsrcdir)/build/package/$(NSARCH)$(OBJDIR_TAG).OBJ/mstone
endif


########################################################################

# setup OS specific compilers and options

CC		= cc
AR		= ar
INCLUDES	= -I$(OBJDIR)
REL_OS_CFLAGS	= -O
REL_OS_LFLAGS	=
DBG_OS_CFLAGS	= -g -D_DEBUG
DBG_OS_LFLAGS	=
LIBS		= -lm
OBJ_SUFFIX	= o
LIB_SUFFIX	= a
EXE_SUFFIX	=
ECHO		= /bin/echo

ifeq ($(OS_ARCH), WINNT)
	CC	= cl
	OSDEFS	= -DWIN32 -D_WIN32
	LIBS	= wsock32.lib libcmt.lib msvcrt.lib
	REL_OS_CFLAGS =
	REL_OS_LINKFLAGS = /link
	DBG_OS_CFLAGS = -Od -Zi
	DBG_OS_LINKFLAGS = /link /debug:full
	OBJ_SUFFIX = obj
	LIB_SUFFIX = .lib
	DLL_SUFFIX = .dll
	EXE_SUFFIX = .exe
	PERL_OS = MSWin32-x86
	# build perl manually, install to c:\perl.  Then build everything else
	#  cd win32 && nmake && nmake install
	PERL5_IMPORT	= c:/perl/$(PERL_REV)/
	PERL_FILES = $(PERL_DIR)/Artistic
	PERL_BIN_FILES = \
	  $(PERL5_IMPORT)/bin/$(PERL_OS)/perl$(EXE_SUFFIX)
	  $(PERL5_IMPORT)/bin/$(PERL_OS)/perl$(DLL_SUFFIX) \
	PERL_LIB_FILES = $(PERL5_IMPORT)/lib/*.pm $(PERL5_IMPORT)/lib/*.pl
	PERL_LIB_OS_FILES = $(PERL5_IMPORT)/lib/$(PERL_OS)/*.pm
endif
ifeq ($(OS_ARCH), IRIX64)
	ARCH	= IRIX
endif
ifeq ($(OS_ARCH), IRIX)
	# MIPSpro Compilers: Version 7.2.1
	CC	= /usr/bin/cc -n32
	REL_OS_CFLAGS = -fullwarn
	DBG_OS_CFLAGS = -fullwarn
	OSDEFS	= -D__IRIX__ -DHAVE_SELECT_H -DHAVE_WAIT_H -DUSE_PTHREADS -DUSE_LRAND48
	LIBS	= -lm -lpthread
	# OS specific flags for perl Configure
	PERL_OS_CONFIGURE = -Dnm=/usr/bin/nm -Dar=/usr/bin/ar
	PERL_OS = IP27-irix
endif
ifeq ($(OS_ARCH), OSF1)
	# DEC C V5.6-071 on Digital UNIX V4.0(D) (Rev. 878)
	CC	= /usr/bin/cc
	REL_OS_CFLAGS = -warnprotos -verbose -newc -std1 -pthread -w0 -readonly_strings
	DBG_OS_CFLAGS = -warnprotos -verbose -newc -std1 -pthread -w0 -readonly_strings
	OSDEFS	= -D__OSF1__ -DHAVE_SELECT_H -DHAVE_WAIT_H -DUSE_PTHREADS -DUSE_LRAND48_R
	LIBS	= -lm -lpthread
	PERL_OS = alpha-dec_osf
endif
ifeq ($(OS_ARCH), AIX)
	CC	= /usr/bin/xlc_r
	REL_OS_CFLAGS = -qro -qroconst -qfullpath -qsrcmsg #-qflag=I:W
	DBG_OS_CFLAGS = -qro -qroconst -g -qfullpath -qsrcmsg #-qflag=I:W
	OSDEFS	= -D__AIX__ -DHAVE_SELECT_H -D_THREAD_SAFE -DUSE_PTHREADS -DUSE_LRAND48_R
	LIBS	= -lm #-lpthread
	PERL_OS = aix
endif
ifeq ($(OS_ARCH), HP-UX)
	CC	= /usr/bin/cc
	# old flags: -Ae +DA1.0 +ESlit
	REL_OS_CFLAGS = +DAportable +DS2.0 -Ae +ESlit
	DBG_OS_CFLAGS = +Z +DAportable +DS2.0 -g -Ae +ESlit
	OSDEFS	= -D__HPUX__ -DUSE_PTHREADS -DUSE_LRAND48
	LIBS	= -lm -lpthread
	PERL_OS = PA-RISC2.0
endif
ifeq ($(OS_ARCH), SunOS)
	# Sun Workshop Compilers 5.0
#	CC	= /tools/ns/workshop-5.0/bin/cc
	REL_OS_CFLAGS = -mt -xstrconst -v -O
	DBG_OS_CFLAGS = -mt -xstrconst -v -g -xs
	OSDEFS	= -D__SOLARIS__ -DHAVE_SELECT_H -DHAVE_WAIT_H \
	 -DXP_UNIX -D_REENTRANT \
	 -DUSE_PTHREADS -DUSE_GETHOSTBYNAME_R -DUSE_GETPROTOBYNAME_R -DUSE_LRAND48
	LIBS    = -lm -lnsl -lsocket -lpthread
	PERL_OS = sun4-solaris
endif
ifeq ($(OS_ARCH), Linux)
	# Linux 2.1 kernels and above
	CC	= /usr/bin/gcc   # gcc 2.7.2.3
	REL_OS_CFLAGS = -O -g -Wall
	DBG_OS_CFLAGS = -O1 -g -Wall
	OSDEFS	= -D__LINUX__ -DHAVE_SELECT_H -DHAVE_WAIT_H -DUSE_PTHREADS -DUSE_LRAND48
	LIBS	= -lm -lpthread
	# Must explicitly enable interpretation of \n
	# works for /bin/echo, sh:echo, or pdksh:echo. NOT tcsh:echo
	ECHO	= /bin/echo -e
	PERL_OS = i686-linux
endif

# pull in any OS extra config, if available
-include $(topsrcdir)/config/$(OS_ARCH)/config.mk

ifeq ($(BUILD_TYPE), DEBUG)
	OS_CFLAGS = $(DBG_OS_CFLAGS) -D_DEBUG
	OS_LINKFLAGS = $(DBG_OS_LINKFLAGS)
else
	OS_CFLAGS = $(REL_OS_CFLAGS)
	OS_LINKFLAGS = $(REL_OS_CFLAGS)
endif

CPPFLAGS	= 
CFLAGS		= $(OS_CFLAGS)
###DEFINES		= -DHAVE_CONFIG_H $(OSDEFS)
DEFINES		= $(OSDEFS)
LDFLAGS         =

CP		= cp
RM		= rm -f

COMPILE         = $(CC) $(CFLAGS) $(DEFINES) $(CPPFLAGS) $(INCLUDES)
ifeq ($(BUILD_VARIANT),release)
STRIP := strip
else
STRIP := true
endif