#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

# Sun's WorkShop defines v8, v8plus and v9 architectures.
# gcc on Solaris defines v8 and v9 "cpus".  
# gcc's v9 is equivalent to Workshop's v8plus.
# gcc's -m64 is equivalent to Workshop's v9

ifeq ($(USE_64), 1)
  ifdef NS_USE_GCC
      ARCHFLAG=-m64
  else
      ifeq ($(OS_TEST),i86pc)
        ARCHFLAG=-xarch=amd64
      else
        ARCHFLAG=-xarch=v9
      endif
  endif
else
  ifneq ($(OS_TEST),i86pc)
    ifdef NS_USE_GCC
      ARCHFLAG=-mcpu=v9
    else
      ARCHFLAG=-xarch=v8plus
    endif
  endif
endif

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC         = gcc
	OS_CFLAGS += -Wall -Wno-format -Werror-implicit-function-declaration -Wno-switch
	OS_CFLAGS += -D__EXTENSIONS__
	CCC        = g++
	CCC       += -Wall -Wno-format
	ASFLAGS	  += -x assembler-with-cpp
	OS_CFLAGS += $(NOMD_OS_CFLAGS) $(ARCHFLAG)
	ifdef BUILD_OPT
	    OPTIMIZER = -O2
	    # Enable this for accurate dtrace profiling
	    # OPTIMIZER += -mno-omit-leaf-frame-pointer -fno-omit-frame-pointer
	endif
else
	CC         = cc
	CCC        = CC
	ASFLAGS   += -Wa,-P
	OS_CFLAGS += $(NOMD_OS_CFLAGS) $(ARCHFLAG)
	ifndef BUILD_OPT
		OS_CFLAGS  += -xs
	else
		OPTIMIZER = -xO4
	endif
	ifdef USE_TCOV
		CC += -xprofile=tcov
		CCC += -xprofile=tcov
	endif
endif

RANLIB      = echo
CPU_ARCH    = sparc
OS_DEFINES += -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS -D_REENTRANT

ifeq ($(OS_TEST),i86pc)
ifeq ($(USE_64),1)
    CPU_ARCH		= x86_64
else
    CPU_ARCH		= x86
    OS_DEFINES		+= -Di386
endif
endif

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS += $(DSO_CFLAGS) $(OS_DEFINES) $(SOL_CFLAGS)

MKSHLIB  = $(CC) $(DSO_LDOPTS) $(RPATH)
ifdef NS_USE_GCC
ifeq (GNU,$(findstring GNU,$(shell `$(CC) -print-prog-name=ld` -v 2>&1)))
	GCC_USE_GNU_LD = 1
endif
endif
ifdef MAPFILE
ifdef NS_USE_GCC
ifdef GCC_USE_GNU_LD
    MKSHLIB += -Wl,--version-script,$(MAPFILE)
else
    MKSHLIB += -Wl,-M,$(MAPFILE)
endif
else
    MKSHLIB += -M $(MAPFILE)
endif
endif
PROCESS_MAP_FILE = grep -v ';-' $< | \
         sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@

# ld options:
# -G: produce a shared object
# -z defs: no unresolved symbols allowed
ifdef NS_USE_GCC
ifeq ($(USE_64), 1)
	DSO_LDOPTS += -m64
endif
	DSO_LDOPTS += -shared -h $(notdir $@)
else
ifeq ($(USE_64), 1)
	ifeq ($(OS_TEST),i86pc)
	    DSO_LDOPTS +=-xarch=amd64
	else
	    DSO_LDOPTS +=-xarch=v9
	endif
endif
	DSO_LDOPTS += -G -h $(notdir $@)
endif
DSO_LDOPTS += -z combreloc -z defs -z ignore

# -KPIC generates position independent code for use in shared libraries.
# (Similarly for -fPIC in case of gcc.)
ifdef NS_USE_GCC
	DSO_CFLAGS += -fPIC
else
	DSO_CFLAGS += -KPIC
endif

NOSUCHFILE   = /solaris-rm-f-sucks

ifeq ($(BUILD_SUN_PKG), 1)
# The -R '$ORIGIN' linker option instructs this library to search for its
# dependencies in the same directory where it resides.
ifeq ($(USE_64), 1)
RPATH = -R '$$ORIGIN:/usr/lib/mps/secv1/64:/usr/lib/mps/64'
else
RPATH = -R '$$ORIGIN:/usr/lib/mps/secv1:/usr/lib/mps'
endif
else
RPATH = -R '$$ORIGIN'
endif

OS_LIBS += -lthread -lnsl -lsocket -lposix4 -ldl -lc
