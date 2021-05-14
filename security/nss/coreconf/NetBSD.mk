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

CPU_ARCH		:= $(shell uname -p)
ifeq ($(CPU_ARCH),i386)
OS_REL_CFLAGS		= -Di386
CPU_ARCH		= x86
endif
ifeq (,$(filter-out earm%,$(CPU_ARCH)))
CPU_ARCH		= arm
endif
ifeq ($(CPU_ARCH),aarch64eb)
CPU_ARCH		= aarch64
endif

DLL_SUFFIX		= so

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -Wno-switch -pipe -DNETBSD -Dunix -DHAVE_STRERROR -DHAVE_BSD_FLOCK

OS_LIBS			= -lcompat

ARCH			= netbsd

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared -Wl,-soname,lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)

#
# The default implementation strategy for NetBSD is pthreads.
#
ifndef CLASSIC_NSPR
USE_PTHREADS		= 1
DEFINES			+= -D_THREAD_SAFE -D_REENTRANT
OS_LIBS			+= -pthread
DSO_LDOPTS		+= -pthread
endif

ifdef LIBRUNPATH
DSO_LDOPTS		+= -Wl,-R$(LIBRUNPATH)
endif

MKSHLIB			= $(CC) $(DSO_LDOPTS)
ifdef MAPFILE
	MKSHLIB += -Wl,--version-script,$(MAPFILE)
endif
PROCESS_MAP_FILE = grep -v ';-' $< | \
        sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@

