#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "NPL"); you may not use this file except in
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

#
# Config stuff for SunOS 5.x on sparc and x86
#

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
LOCAL_THREADS_ONLY = 1
endif
ifndef NS_USE_NATIVE
NS_USE_GCC = 1
endif
endif

#
# The default implementation strategy on Solaris is pthreads.
# Global threads only and local threads only are also available.
#
ifeq ($(GLOBAL_THREADS_ONLY),1)
  IMPL_STRATEGY = _NATIVE
  DEFINES += -D_PR_GLOBAL_THREADS_ONLY
else
  ifeq ($(LOCAL_THREADS_ONLY),1)
    IMPL_STRATEGY = _EMU
    DEFINES += -D_PR_LOCAL_THREADS_ONLY
  else
    USE_PTHREADS = 1
    IMPL_STRATEGY = _PTH
  endif
endif

ifdef NS_USE_GCC
CC			= gcc -Wall
CCC			= g++ -Wall
COMPILER_TAG		= _gcc
ifdef NO_MDUPDATE
OS_CFLAGS		= $(NOMD_OS_CFLAGS)
else
OS_CFLAGS		= $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
endif
else
CC			= cc -xstrconst
CCC			= CC -Qoption cg -xstrconst
ASFLAGS			+= -Wa,-P
OS_CFLAGS		= $(NOMD_OS_CFLAGS)
#
# If we are building for a release, we want to put all symbol
# tables in the debug executable or share library instead of
# the .o files, so that our clients can run dbx on the debug
# library without having the .o files around.
#
ifdef BUILD_NUMBER
ifndef BUILD_OPT
OS_CFLAGS		+= -xs
endif
endif
endif

RANLIB			= echo

OS_DEFINES		= -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS

ifeq ($(OS_TEST),i86pc)
CPU_ARCH		= x86
OS_DEFINES		+= -Di386
# The default debug format, DWARF (-g), is not supported by gcc
# on i386-ANY-sysv4/solaris, but the stabs format is.  It is
# assumed that the Solaris assembler /usr/ccs/bin/as is used.
# If your gcc uses GNU as, you do not need the -Wa,-s option.
ifndef BUILD_OPT
ifdef NS_USE_GCC
OPTIMIZER		= -Wa,-s -gstabs
endif
endif
else
CPU_ARCH		= sparc
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

ifeq (5.5,$(findstring 5.5,$(OS_RELEASE)))
OS_DEFINES		+= -DSOLARIS2_5
else
ifeq (,$(filter-out 5.3 5.4,$(OS_RELEASE)))
OS_DEFINES		+= -D_PR_NO_LARGE_FILES
else
OS_DEFINES		+= -D_PR_HAVE_OFF64_T
endif
endif

ifneq ($(LOCAL_THREADS_ONLY),1)
OS_DEFINES		+= -D_REENTRANT -DHAVE_POINTER_LOCALTIME_R
endif

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS		= $(DSO_CFLAGS) $(OS_DEFINES) $(SOL_CFLAGS)

MKSHLIB			= $(LD) $(DSO_LDOPTS)

# ld options:
# -G: produce a shared object
# -z defs: no unresolved symbols allowed
DSO_LDOPTS		= -G

# -KPIC generates position independent code for use in shared libraries.
# (Similarly for -fPIC in case of gcc.)
ifdef NS_USE_GCC
DSO_CFLAGS		= -fPIC
else
DSO_CFLAGS		= -KPIC
endif

HAVE_PURIFY		= 1

NOSUCHFILE		= /no-such-file

#
# Library of atomic functions for UltraSparc systems
#
# The nspr makefiles build ULTRASPARC_LIBRARY (which contains assembly language
# implementation of the nspr atomic functions for UltraSparc systems) in addition
# to libnspr.so. (The actual name of the library is
# lib$(ULTRASPARC_LIBRARY)$(MOD_VERSION).so
#
# The actual name of the filter-library, recorded in libnspr.so, is set to the
# value of $(ULTRASPARC_FILTER_LIBRARY).
# For an application to use the assembly-language implementation, a link should be
# made so that opening ULTRASPARC_FILTER_LIBRARY results in opening
# ULTRASPARC_LIBRARY. This indirection requires the user to explicitly set up
# library for use on UltraSparc systems, thereby helping to avoid using it by
# accident on non-UltraSparc systems.
# The directory containing the ultrasparc libraries should be in LD_LIBRARY_PATH.
#
ifeq ($(OS_TEST),sun4u)
ULTRASPARC_LIBRARY = ultrasparc
ULTRASPARC_FILTER_LIBRARY = libatomic.so
DSO_LDOPTS		+= -f $(ULTRASPARC_FILTER_LIBRARY)
endif
