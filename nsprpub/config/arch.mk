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

#######################################################################
# Master "Core Components" macros for getting the OS architecture     #
#######################################################################

#
# Macros for getting the OS architecture
#

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
	OS_ARCH := IRIX
endif

#
# Force the newer BSDI versions to use the old arch name.
#

ifeq ($(OS_ARCH),BSD_OS)
	OS_ARCH := BSD_386
endif

#
# Drop all but the major revision for FreeBSD releases.
#
# Handle one dot (2.2-STABLE) and two dot (2.2.5-RELEASE) forms.
#

ifeq ($(OS_ARCH),FreeBSD)
	OS_RELEASE := $(basename $(shell uname -r))
	NEW_OS_RELEASE := $(basename $(OS_RELEASE))
	ifneq ($(NEW_OS_RELEASE),"")
		OS_RELEASE := $(NEW_OS_RELEASE)
	endif
endif

#
# Catch NCR butchering of SVR4
#

ifeq ($(OS_ARCH),UNIX_SV)
	ifneq ($(findstring NCR, $(shell grep NCR /etc/bcheckrc | head -1 )),)
		OS_ARCH := NCR
	else
		# Make UnixWare something human readable
		OS_ARCH := UNIXWARE
	endif

	# Get the OS release number, not 4.2
	OS_RELEASE := $(shell uname -v)
endif

ifeq ($(OS_ARCH),UNIX_System_V)
	OS_ARCH	:= NEC
endif

ifeq ($(OS_ARCH),AIX)
	OS_RELEASE := $(shell uname -v).$(shell uname -r)
endif

ifeq (osfmach3,$(findstring osfmach3,$(OS_RELEASE)))
	OS_RELEASE	:= ppc
endif

#
# SINIX changes name to ReliantUNIX with 5.43
#

ifeq ($(OS_ARCH),ReliantUNIX-N)
	OS_ARCH    := ReliantUNIX
	OS_RELEASE := 5.4
endif

ifeq ($(OS_ARCH),SINIX-N)
	OS_ARCH    := ReliantUNIX
	OS_RELEASE := 5.4
endif

#
# OS_OBJTYPE is used only by Linux
#

ifeq ($(OS_ARCH),Linux)
	OS_OBJTYPE := ELF
	OS_RELEASE := $(basename $(OS_RELEASE))
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
ifeq ($(OS_ARCH), OS2)
	OS_ARCH   := WINNT
	OS_TARGET := OS2
endif

#
# On WIN32, we also define the variable CPU_ARCH.
#

ifeq ($(OS_ARCH), WINNT)
	CPU_ARCH := $(shell uname -p)
	ifeq ($(CPU_ARCH),I386)
		CPU_ARCH = x386
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
		CPU_ARCH = x386
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
		CPU_ARCH = x386
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

OS_CONFIG := $(OS_TARGET)$(OS_OBJTYPE)$(OS_RELEASE)
