#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

include $(CORE_DEPTH)/coreconf/UNIX.mk

#
# The default implementation strategy for Linux is now pthreads
#
USE_PTHREADS = 1

ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
endif

CC			= gcc
CCC			= g++
RANLIB			= ranlib

DEFAULT_COMPILER = gcc

ifeq ($(OS_TEST),ppc64)
	CPU_ARCH	= ppc
ifeq ($(USE_64),1)
	ARCHFLAG	= -m64
endif
else
ifeq ($(OS_TEST),alpha)
        OS_REL_CFLAGS   = -D_ALPHA_
	CPU_ARCH	= alpha
else
ifeq ($(OS_TEST),x86_64)
ifeq ($(USE_64),1)
	CPU_ARCH	= x86_64
else
	OS_REL_CFLAGS	= -Di386
	CPU_ARCH	= x86
	ARCHFLAG	= -m32
endif
else
ifeq ($(OS_TEST),sparc64)
	CPU_ARCH        = sparc
else
ifeq (,$(filter-out arm% sa110,$(OS_TEST)))
	CPU_ARCH        = arm
else
ifeq (,$(filter-out parisc%,$(OS_TEST)))
	CPU_ARCH        = hppa
else
ifeq (,$(filter-out i%86,$(OS_TEST)))
	OS_REL_CFLAGS	= -Di386
	CPU_ARCH	= x86
else
ifeq ($(OS_TEST),sh4a)
	CPU_ARCH        = sh4
else
# $(OS_TEST) == m68k, ppc, ia64, sparc, s390, s390x, mips, sh3, sh4
	CPU_ARCH	= $(OS_TEST)
endif
endif
endif
endif
endif
endif
endif
endif


LIBC_TAG		= _glibc

ifeq ($(OS_RELEASE),2.0)
	OS_REL_CFLAGS	+= -DLINUX2_0
	MKSHLIB		= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so) $(RPATH)
	ifdef MAPFILE
		MKSHLIB += -Wl,--version-script,$(MAPFILE)
	endif
	PROCESS_MAP_FILE = grep -v ';-' $< | \
         sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@
endif

ifdef BUILD_OPT
ifeq (11,$(ALLOW_OPT_CODE_SIZE)$(OPT_CODE_SIZE))
	OPTIMIZER = -Os
else
	OPTIMIZER = -O2
endif
ifdef MOZ_DEBUG_SYMBOLS
	ifdef MOZ_DEBUG_FLAGS
		OPTIMIZER += $(MOZ_DEBUG_FLAGS)
	else
		OPTIMIZER += -gdwarf-2
	endif
endif
endif


ifeq ($(USE_PTHREADS),1)
OS_PTHREAD = -lpthread 
endif

# See bug 537829, in particular comment 23.
# Place -ansi and *_SOURCE before $(DSO_CFLAGS) so DSO_CFLAGS can override
# -ansi on platforms like Android where the system headers are C99 and do
# not build with -ansi.
STANDARDS_CFLAGS	= -ansi -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE
OS_CFLAGS		= $(STANDARDS_CFLAGS) $(DSO_CFLAGS) $(OS_REL_CFLAGS) $(ARCHFLAG) -Wall -Werror-implicit-function-declaration -Wno-switch -pipe -DLINUX -Dlinux -DHAVE_STRERROR
OS_LIBS			= $(OS_PTHREAD) -ldl -lc

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT
endif

ARCH			= linux

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared $(ARCHFLAG)
# The linker on Red Hat Linux 7.2 and RHEL 2.1 (GNU ld version 2.11.90.0.8)
# incorrectly reports undefined references in the libraries we link with, so
# we don't use -z defs there.
ZDEFS_FLAG		= -Wl,-z,defs
DSO_LDOPTS		+= $(if $(findstring 2.11.90.0.8,$(shell ld -v)),,$(ZDEFS_FLAG))
LDFLAGS			+= $(ARCHFLAG)

# On Maemo, we need to use the -rpath-link flag for even the standard system
# library directories.
ifdef _SBOX_DIR
LDFLAGS			+= -Wl,-rpath-link,/usr/lib:/lib
endif

# INCLUDES += -I/usr/include -Y/usr/include/linux
G++INCLUDES		= -I/usr/include/g++

#
# Always set CPU_TAG on Linux, WINCE.
#
CPU_TAG = _$(CPU_ARCH)

#
# On Linux 2.6 or later, build libfreebl3.so with no NSPR and libnssutil3.so
# dependencies by default.  Set FREEBL_NO_DEPEND to 0 in the environment to
# override this.
#
ifeq (2.6,$(firstword $(sort 2.6 $(OS_RELEASE))))
ifndef FREEBL_NO_DEPEND
FREEBL_NO_DEPEND = 1
endif
endif

USE_SYSTEM_ZLIB = 1
ZLIB_LIBS = -lz

# The -rpath '$$ORIGIN' linker option instructs this library to search for its
# dependencies in the same directory where it resides.
ifeq ($(BUILD_SUN_PKG), 1)
ifeq ($(USE_64), 1)
RPATH = -Wl,-rpath,'$$ORIGIN:/opt/sun/private/lib64:/opt/sun/private/lib'
else
RPATH = -Wl,-rpath,'$$ORIGIN:/opt/sun/private/lib'
endif
endif

OS_REL_CFLAGS   += -DLINUX2_1
MKSHLIB         = $(CC) $(DSO_LDOPTS) -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so) $(RPATH)

ifdef MAPFILE
	MKSHLIB += -Wl,--version-script,$(MAPFILE)
endif
PROCESS_MAP_FILE = grep -v ';-' $< | \
        sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@

ifeq ($(OS_RELEASE),2.4)
DEFINES += -DNO_FORK_CHECK
endif
