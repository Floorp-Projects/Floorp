#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

CC			?= gcc
CXX			?= g++
DEFAULT_COMPILER	= ${CC}
CCC			= ${CXX}
RANLIB			= ranlib

CPU_ARCH		:= $(shell arch -s)
ifeq ($(CPU_ARCH),i386)
OS_REL_CFLAGS		= -Di386
CPU_ARCH		= x86
endif

ifndef CLASSIC_NSPR
USE_PTHREADS		= 1
DEFINES			+= -pthread
OS_LIBS			+= -pthread
DSO_LDOPTS		+= -pthread
endif

DLL_SUFFIX		= so.1.0

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -Wno-switch -pipe -DOPENBSD

OS_LIBS			= 

ARCH			= openbsd

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared -fPIC -Wl,-soname,lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)

MKSHLIB			= $(CC) $(DSO_LDOPTS)

USE_SYSTEM_ZLIB		= 1
ZLIB_LIBS		= -lz
