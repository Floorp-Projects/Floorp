#!/bin/sh
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

#######################################################################
# Master "Core Components" macros for getting the OS architecture     #
#######################################################################

#
# Macros for getting the OS architecture
#
if [ -n "${USE_64}" ]; then
	A64BIT_TAG=_64
else
	A64BIT_TAG=
fi

#OS_ARCH := $(subst /,_,$(shell uname -s))
OS_ARCH=`uname -s | sed -e 's;/;_;'`

#
# Attempt to differentiate between sparc and x86 Solaris
#

#OS_TEST := $(shell uname -m)
OS_TEST=`uname -m`
if [ ${OS_TEST} = "i86pc" ]; then
	OS_RELEASE=`uname -r`"_"${OS_TEST}
else
	OS_RELEASE=`uname -r`
fi

#
# Force the IRIX64 machines to use IRIX.
#
if [ ${OS_ARCH} = "IRIX64" ]; then
	OS_ARCH="IRIX"
fi

#
# Force the newer BSDI versions to use the old arch name.
#

if [ ${OS_ARCH} = "BSD_OS" ]; then
	OS_ARCH=BSD_386
fi

#
# Catch Deterim if SVR4 is NCR or UNIXWARE
#

if  [ ${OS_ARCH} = "UNIX_SV" ]; then
	if grep NCR /etc/bcheckrc ; then
		OS_ARCH=NCR
	else
		# Make UnixWare something human readable
		OS_ARCH=UNIXWARE
	fi

	# Get the OS release number, not 4.2
	OS_RELEASE=`uname -v`
fi

if [ ${OS_ARCH} = "UNIX_System_V" ]; then
	OS_ARCH=NEC
fi

if [ ${OS_ARCH} = "AIX" ]; then
	OS_MAJOR=`uname -v`
	OS_MINOR=`uname -r`
	OS_RELEASE=${OS_MAJOR}.${OS_MINOR}
fi

#
# Distinguish between OSF1 V4.0B and V4.0D
#

if  [ ${OS_ARCH}${OS_RELEASE} = "OSF1V4.0" ]; then
	OS_VERSION=`uname -v`
	if [ ${OS_VERSION} = "564" ]; then
		OS_RELEASE=V4.0B
	fi
	if [ ${OS_VERSION} = "878" ]; then
		OS_RELEASE=V4.0D
	fi
fi

#
# SINIX changes name to ReliantUNIX with 5.43
#

if  [ ${OS_ARCH} = "ReliantUNIX-N" ]; then
	OS_ARCH=ReliantUNIX
	OS_RELEASE=5.4
fi

if [ ${OS_ARCH} = "SINIX-N" ]; then
	OS_ARCH=ReliantUNIX
	OS_RELEASE=5.4
fi

#
# Handle FreeBSD 2.2-STABLE and Linux 2.0.30-osfmach3
#

#if [(,$(filter-out Linux FreeBSD,${OS_ARCH}))
#OS_RELEASE	:= $(shell echo $(OS_RELEASE) | sed 's/-.*//')
#endif
if [  ${OS_ARCH} = "Linux" ]; then
	OS_RELEASE=`echo ${OS_RELEASE} | sed 's/-.*//'`
fi

if  [ ${OS_ARCH} = "Linux" ]; then
	OS_RELEASE=`echo ${OS_RELEASE} | sed 's;\\.[0123456789]*$;;'`
fi

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
# If OS_TARGET is not specified, it defaults to ${OS_ARCH}, i.e., no
# cross-compilation.
#

#
# The following hack allows one to build on a WIN95 machine (as if
# s/he were cross-compiling on a WINNT host for a WIN95 target).
# It also accomodates for MKS's uname.exe.  If you never intend
# to do development on a WIN95 machine, you don't need this. It doesn't
# work any more anyway.
#
if [ ${OS_ARCH} = "WIN95" ]; then
	OS_ARCH=WINNT
	OS_TARGET=WIN95
fi
if [ ${OS_ARCH} = "Windows_95" ]; then
	OS_ARCH=Windows_NT
	OS_TARGET=WIN95
fi

#
# On WIN32, we also define the variable CPU_ARCH.
#

if  [ ${OS_ARCH} = "WINNT" ]; then
	CPU_ARCH=`uname -p`
	if [ ${CPU_ARCH} = "I386" ]; then
		CPU_ARCH=x386
	fi
else
#
# If uname -s returns "Windows_NT", we assume that we are using
# the uname.exe in MKS toolkit.
#
# The -r option of MKS uname only returns the major version number.
# So we need to use its -v option to get the minor version number.
# Moreover, it doesn't have the -p option, so we need to use uname -m.
#
if  [ ${OS_ARCH}  = "Windows_NT" ]; then
	OS_ARCH=WINNT
	OS_MINOR_RELEASE=`uname -v`
	if [ ${OS_MINOR_RELEASE} = "00" ]; then
		OS_MINOR_RELEASE=0
	fi
	OS_RELEASE=${OS_RELEASE}.${OS_MINOR_RELEASE}
	CPU_ARCH=`uname -m`
	#
	# MKS's uname -m returns "586" on a Pentium machine.
	#
	#ifneq (,$(findstring 86,$(CPU_ARCH)))
	if (echo $CPU_ARCH | grep 86) ; then
		CPU_ARCH=x386
	fi
fi
fi

if  [ ${OS_ARCH} = "Linux" ]; then
	IMPL_STRATEGY=_PTH

	if [ ${OS_TEST} = "ppc" ]; then
		CPU_TAG=_ppc
	elif [ ${OS_TEST} = "alpha" ]; then
		CPU_TAG=_alpha
	elif [ ${OS_TEST} = "ia64" ]; then
		CPU_TAG=_ia64
	else
		CPU_TAG=_x86
	fi
	LIBC_TAG=_glibc
	ARCH=linux
fi

OS_TARGET=${OS_TARGET-${OS_ARCH}}

if [ ${OS_TARGET} = "WIN95" ]; then
	OS_RELEASE=4.0
fi

if [ ${OS_TARGET} = "WIN16" ]; then
	OS_RELEASE=
#	OS_RELEASE = _3.11
fi

#
# This variable is used to get OS_CONFIG.mk.
#

OS_CONFIG=${OS_TARGET}${OS_RELEASE}

#
# OBJDIR_TAG depends on the predefined variable BUILD_OPT,
# to distinguish between debug and release builds.
#

if [ -n "${BUILD_OPT}" ]; then
	if [ ${OS_TARGET} = "WIN16" ]; then
		OBJDIR_TAG=_O
	else
		OBJDIR_TAG=${A64BIT_TAG}_OPT
	fi
else
	if [ -n "${BUILD_IDG}" ]; then
		if [ ${OS_TARGET} = "WIN16" ]; then
			OBJDIR_TAG=_I
		else
			OBJDIR_TAG=${A64BIT_TAG}_IDG
		fi
	else
		if [ ${OS_TARGET} = WIN16 ]; then
			OBJDIR_TAG=_D
		else
			OBJDIR_TAG=${A64BIT_TAG}_DBG
		fi
	fi
fi

#
# The following flags are defined in the individual $(OS_CONFIG).mk
# files.
#
# CPU_TAG is defined if the CPU is not the most common CPU.
# COMPILER_TAG is defined if the compiler is not the native compiler.
# IMPL_STRATEGY may be defined too.
#

# Name of the binary code directories
#ifeq (${OS_ARCH}, WINNT)
#	ifeq ($(CPU_ARCH),x386)
##		OBJDIR_NAME=$(OS_CONFIG)$(OBJDIR_TAG).OBJ
#	else
#		OBJDIR_NAME=$(OS_CONFIG)$(CPU_ARCH)$(OBJDIR_TAG).OBJ
#	endif
#else
#endif

OBJDIR_NAME=${OS_CONFIG}${CPU_TAG}${COMPILER_TAG}${LIBC_TAG}${IMPL_STRATEGY}${OBJDIR_TAG}'.OBJ'

#export OS_CONFIG OS_ARCH OBJDIR_NAME OS_RELEASE OBJDIR_TAG
export OS_ARCH

if [ ${OS_ARCH} = "WINNT" ]; then
if [  ${OS_TARGET} != "WIN16" ]; then
if [  -z "${BUILD_OPT}" ]; then
#
# Define USE_DEBUG_RTL if you want to use the debug runtime library
# (RTL) in the debug build
#
if [ -n "${USE_DEBUG_RTL}" ]; then
	OBJDIR_NAME=${OS_CONFIG}${CPU_TAG}${COMPILER_TAG}${IMPL_STRATEGY}${OBJDIR_TAG}.OBJD
fi
fi
fi
fi

