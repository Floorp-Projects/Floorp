#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# Master "Core Components" macros for getting the OS architecture     #
# defines these symbols:
# 64BIT_TAG
# OS_ARCH	(from uname -r)
# OS_TEST	(from uname -m)
# OS_RELEASE	(from uname -v and/or -r)
# OS_TARGET	User defined, or set to OS_ARCH
# CPU_ARCH  	(from unmame -m or -p, ONLY on WINNT)
# OS_CONFIG	OS_TARGET + OS_RELEASE
# OBJDIR_TAG
# OBJDIR_NAME
#######################################################################

#
# Macros for getting the OS architecture
#

ifeq ($(USE_64), 1)
	64BIT_TAG=_64
else
	64BIT_TAG=
endif

OS_ARCH := $(subst /,_,$(shell uname -s))

#
# Attempt to differentiate between sparc and x86 Solaris
#

OS_TEST := $(shell uname -m)
ifeq ($(OS_TEST),i86pc)
    OS_RELEASE := $(shell uname -r)_$(OS_TEST)
else
    OS_RELEASE := $(shell uname -r)
endif

#
# Force the IRIX64 machines to use IRIX.
#

ifeq ($(OS_ARCH),IRIX64)
    OS_ARCH = IRIX
endif

#
# Force the older BSD/OS versions to use the new arch name.
#

ifeq ($(OS_ARCH),BSD_386)
    OS_ARCH = BSD_OS
endif

#
# Catch Deterim if SVR4 is NCR or UNIXWARE
#

ifeq ($(OS_ARCH),UNIX_SV)
    ifneq ($(findstring NCR, $(shell grep NCR /etc/bcheckrc | head -1 )),)
	OS_ARCH = NCR
    else
	# Make UnixWare something human readable
	OS_ARCH = UNIXWARE
    endif

    # Get the OS release number, not 4.2
    OS_RELEASE := $(shell uname -v)
endif

ifeq ($(OS_ARCH),UNIX_System_V)
    OS_ARCH	= NEC
endif

ifeq ($(OS_ARCH),AIX)
    OS_RELEASE := $(shell uname -v).$(shell uname -r)
endif

#
# Distinguish between OSF1 V4.0B and V4.0D
#

ifeq ($(OS_ARCH)$(OS_RELEASE),OSF1V4.0)
    OS_VERSION := $(shell uname -v)
    ifeq ($(OS_VERSION),564)
	OS_RELEASE := V4.0B
    endif
    ifeq ($(OS_VERSION),878)
	OS_RELEASE := V4.0D
    endif
endif

#
# SINIX changes name to ReliantUNIX with 5.43
#

ifeq ($(OS_ARCH),ReliantUNIX-N)
    OS_ARCH    = ReliantUNIX
    OS_RELEASE = 5.4
endif

ifeq ($(OS_ARCH),SINIX-N)
    OS_ARCH    = ReliantUNIX
    OS_RELEASE = 5.4
endif

#
# Handle FreeBSD 2.2-STABLE, Linux 2.0.30-osfmach3, and
# IRIX 6.5-ALPHA-1289139620.
#

ifeq (,$(filter-out Linux FreeBSD IRIX,$(OS_ARCH)))
    OS_RELEASE := $(shell echo $(OS_RELEASE) | sed 's/-.*//')
endif

ifeq ($(OS_ARCH),Linux)
    OS_RELEASE := $(subst ., ,$(OS_RELEASE))
    ifneq ($(words $(OS_RELEASE)),1)
	OS_RELEASE := $(word 1,$(OS_RELEASE)).$(word 2,$(OS_RELEASE))
    endif
endif

#
# For OS/2
#
ifeq ($(OS_ARCH),OS_2)
    OS_ARCH = OS2
    OS_RELEASE := $(shell uname -v)
endif

#######################################################################
# Master "Core Components" macros for getting the OS target           #
#######################################################################

#
# Note: OS_TARGET should be specified on the command line for gmake.
# When OS_TARGET=WIN95 is specified, then a Windows 95 target is built.
# The difference between the Win95 target and the WinNT target is that
# the WinNT target uses Windows NT specific features not available
# in Windows 95. The Win95 target will run on Windows NT, but (supposedly)
# at lesser performance (the Win95 target uses threads; the WinNT target
# uses fibers).
#
# If OS_TARGET is not specified, it defaults to $(OS_ARCH), i.e., no
# cross-compilation.
#

#
# The following hack allows one to build on a WIN95 machine (as if
# s/he were cross-compiling on a WINNT host for a WIN95 target).
# It also accomodates for MKS's and Cygwin's uname.exe.
#
ifeq ($(OS_ARCH),WIN95)
    OS_ARCH   = WINNT
    OS_TARGET = WIN95
endif
ifeq ($(OS_ARCH),Windows_95)
    OS_ARCH   = Windows_NT
    OS_TARGET = WIN95
endif
ifeq ($(OS_ARCH),CYGWIN_95-4.0)
	OS_ARCH   = CYGWIN_NT-4.0
	OS_TARGET = WIN95
endif
ifeq ($(OS_ARCH),CYGWIN_98-4.10)
	OS_ARCH   = CYGWIN_NT-4.0
	OS_TARGET = WIN95
endif
ifeq ($(OS_ARCH),CYGWIN_ME-4.90)
	OS_ARCH   = CYGWIN_NT-4.0
	OS_TARGET = WIN95
endif

#
# On WIN32, we also define the variable CPU_ARCH, if it isn't already.
#
ifndef CPU_ARCH
    ifeq ($(OS_ARCH), WINNT)
	CPU_ARCH := $(shell uname -p)
	ifeq ($(CPU_ARCH),I386)
	    CPU_ARCH = x386
	endif
    endif
endif

# If uname -s returns "Windows_NT", we assume that we are using
# the uname.exe in MKS toolkit.
#
# The -r option of MKS uname only returns the major version number.
# So we need to use its -v option to get the minor version number.
# Moreover, it doesn't have the -p option, so we need to use uname -m.
#
ifeq ($(OS_ARCH), Windows_NT)
    OS_ARCH = WINNT
    OS_MINOR_RELEASE := $(shell uname -v)
    # strip leading 0
    OS_MINOR_RELEASE := $(patsubst 0%,%,$(OS_MINOR_RELEASE))
    OS_RELEASE := $(OS_RELEASE).$(OS_MINOR_RELEASE)
    ifndef CPU_ARCH
	CPU_ARCH := $(shell uname -m)
	#
	# MKS's uname -m returns "586" on a Pentium machine.
	#
	ifneq (,$(findstring 86,$(CPU_ARCH)))
	    CPU_ARCH = x386
	endif
    endif
endif
#
# If uname -s returns "CYGWIN_NT-4.0", we assume that we are using
# the uname.exe in the Cygwin tools.
#
ifeq (CYGWIN_NT,$(findstring CYGWIN_NT,$(OS_ARCH)))
    OS_RELEASE := $(patsubst CYGWIN_NT-%,%,$(OS_ARCH))
    OS_ARCH = WINNT
    ifndef CPU_ARCH
    ifeq (WOW64,$(findstring WOW64,$(OS_RELEASE)))
        OS_RELEASE := $(patsubst %-WOW64,%,$(OS_RELEASE))
    endif    
	CPU_ARCH := $(shell uname -m)
	#
	# Cygwin's uname -m returns "i686" on a Pentium Pro machine.
	#
	ifneq (,$(findstring 86,$(CPU_ARCH)))
	    CPU_ARCH = x386
	endif
    endif
endif
#
# If uname -s returns "MINGW32_NT-5.1", we assume that we are using
# the uname.exe in the MSYS toolkit.
#
ifeq (MINGW32_NT,$(findstring MINGW32_NT,$(OS_ARCH)))
    OS_RELEASE := $(patsubst MINGW32_NT-%,%,$(OS_ARCH))
    OS_ARCH = WINNT
    USE_MSYS = 1
    ifndef CPU_ARCH
	CPU_ARCH := $(shell uname -m)
	#
	# MSYS's uname -m returns "i686" on a Pentium Pro machine.
	#
	ifneq (,$(findstring 86,$(CPU_ARCH)))
	    CPU_ARCH = x386
	endif
    endif
endif

ifndef OS_TARGET
    OS_TARGET = $(OS_ARCH)
endif

ifeq ($(OS_TARGET), WIN95)
    OS_RELEASE = 4.0
endif

ifdef OS_TARGET_RELEASE
    OS_RELEASE = $(OS_TARGET_RELEASE)
endif

#
# This variable is used to get OS_CONFIG.mk.
#

OS_CONFIG = $(OS_TARGET)$(OS_RELEASE)

#
# OBJDIR_TAG depends on the predefined variable BUILD_OPT,
# to distinguish between debug and release builds.
#

ifdef BUILD_OPT
    OBJDIR_TAG = $(64BIT_TAG)_OPT
else
    ifdef BUILD_IDG
	OBJDIR_TAG = $(64BIT_TAG)_IDG
    else
	OBJDIR_TAG = $(64BIT_TAG)_DBG
    endif
endif

#
# The following flags are defined in the individual $(OS_CONFIG).mk
# files.
#
# CPU_TAG is defined if the CPU is not the most common CPU.
# COMPILER_TAG is defined if the compiler is not the default compiler.
# IMPL_STRATEGY may be defined too.
#

OBJDIR_NAME = $(OS_TARGET)$(OS_RELEASE)$(CPU_TAG)$(COMPILER_TAG)$(LIBC_TAG)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJ

ifeq (,$(filter-out WIN%,$(OS_TARGET)))
ifndef BUILD_OPT
#
# Define USE_DEBUG_RTL if you want to use the debug runtime library
# (RTL) in the debug build
#
ifdef USE_DEBUG_RTL
    OBJDIR_NAME = $(OS_TARGET)$(OS_RELEASE)$(CPU_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJD
endif
endif
endif

MK_ARCH = included
