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

#######################################################################
# Master "Core Components" macros for getting the OS architecture     #
#######################################################################

#
# Important internal static macros
#

OS_ARCH		:= $(subst /,_,$(shell uname -s))
OS_RELEASE	:= $(shell uname -r)
OS_TEST		:= $(shell uname -m)

#
# Tweak the default OS_ARCH and OS_RELEASE macros as needed.
#
ifeq ($(OS_ARCH),AIX)
OS_RELEASE	:= $(shell uname -v).$(shell uname -r)
endif
ifeq ($(OS_ARCH),BSD_386)
OS_ARCH		:= BSD_OS
endif
ifeq ($(OS_ARCH),dgux)
OS_ARCH		:= DGUX
endif
ifeq ($(OS_ARCH),IRIX64)
OS_ARCH		:= IRIX
endif
ifeq ($(OS_ARCH),UNIX_SV)
ifneq ($(findstring NCR,$(shell grep NCR /etc/bcheckrc | head -1 )),)
OS_ARCH		:= NCR
else
OS_ARCH		:= UNIXWARE
OS_RELEASE	:= $(shell uname -v)
endif
endif
ifeq ($(OS_ARCH),ncr)
OS_ARCH		:= NCR
endif
# This is the only way to correctly determine the actual OS version on NCR boxes.
ifeq ($(OS_ARCH),NCR)
OS_RELEASE	:= $(shell awk '{print $$3}' /etc/.relid | sed 's/^\([0-9]\)\(.\)\(..\)\(.*\)$$/\2.\3/')
endif
ifeq ($(OS_ARCH),UNIX_System_V)
OS_ARCH		:= NEC
endif
ifeq ($(OS_ARCH),QNX)
OS_RELEASE	:= $(shell uname -v | sed 's/^\([0-9]\)\([0-9]*\)$$/\1.\2/')
endif
ifeq ($(OS_ARCH),SCO_SV)
OS_ARCH		:= SCOOS
OS_RELEASE	:= 5.0
endif
ifeq ($(OS_ARCH),SINIX-N)
OS_ARCH		:= SINIX
endif
ifeq ($(OS_ARCH),SINIX-Y)
OS_ARCH		:= SINIX
endif
ifeq ($(OS_ARCH),SINIX-Z)
OS_ARCH		:= SINIX
endif
# SINIX changes name to ReliantUNIX with 5.43
ifeq ($(OS_ARCH),ReliantUNIX-N)
OS_ARCH		:= SINIX
endif
ifeq ($(OS_ARCH),UnixWare)
OS_ARCH		:= UNIXWARE
OS_RELEASE	:= $(shell uname -v)
endif

#
# Handle FreeBSD 2.2-STABLE and Linux 2.0.30-osfmach3
#

ifeq (,$(filter-out Linux FreeBSD,$(OS_ARCH)))
OS_RELEASE	:= $(shell echo $(OS_RELEASE) | sed 's/-.*//')
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
# Handle uname variants for OS/2.
#

ifeq ($(OS_ARCH),OS_2)
	OS_ARCH		:= OS2
	OS_RELEASE	:= 4.0
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
# When OS_TARGET=WIN16 is specified, then a Windows 3.11 (16bit) target
# is built. See: win16_3.11.mk for lots more about the Win16 target.
#
# If OS_TARGET is not specified, it defaults to $(OS_ARCH), i.e., no
# cross-compilation.
#

#
# The following hack allows one to build on a WIN95 machine (as if
# s/he were cross-compiling on a WINNT host for a WIN95 target).
# It also accomodates for MKS's uname.exe.  If you never intend
# to do development on a WIN95 machine, you don't need this hack.
#
ifeq ($(OS_ARCH),WIN95)
	OS_ARCH   := WINNT
	OS_TARGET := WIN95
endif
ifeq ($(OS_ARCH),Windows_95)
	OS_ARCH   := Windows_NT
	OS_TARGET := WIN95
endif
ifeq ($(OS_ARCH),OS2)
	OS_ARCH   := WINNT
	OS_TARGET := OS2
endif

#
# On WIN32, we also define the variable CPU_ARCH.
#

ifeq ($(OS_ARCH), WINNT)
	ifneq ($(subst /,_,$(shell uname -s)),OS_2)
		CPU_ARCH := $(shell uname -p)
	else
		CPU_ARCH := $(shell uname -m)
	endif
	ifeq ($(CPU_ARCH),I386)
		CPU_ARCH = x86
	endif
else
#
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
	ifeq ($(OS_MINOR_RELEASE),00)
		OS_MINOR_RELEASE = 0
	endif
	OS_RELEASE := $(OS_RELEASE).$(OS_MINOR_RELEASE)
	CPU_ARCH := $(shell uname -m)
	#
	# MKS's uname -m returns "586" on a Pentium machine.
	#
	ifneq (,$(findstring 86,$(CPU_ARCH)))
		CPU_ARCH = x86
	endif
else
#
# If uname -s returns "CYGWIN32/NT", we assume that we are using
# the uname.exe in the GNU-Win32 tools.
#
ifeq ($(OS_ARCH), CYGWIN32_NT)
	OS_ARCH = WINNT
	CPU_ARCH := $(shell uname -m)
	#
	# GNU-Win32's uname -m returns "i686" on a Pentium Pro machine.
	#
	ifneq (,$(findstring 86,$(CPU_ARCH)))
		CPU_ARCH = x86
	endif
endif
endif
endif

ifndef OS_TARGET
	OS_TARGET := $(OS_ARCH)
endif

ifeq ($(OS_TARGET), WIN95)
	OS_RELEASE := 4.0
endif

ifeq ($(OS_TARGET), WIN16)
	OS_RELEASE :=
#	OS_RELEASE := _3.11
endif

OS_CONFIG := $(OS_TARGET)$(OS_RELEASE)
