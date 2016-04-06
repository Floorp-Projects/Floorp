#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER	= gcc
CC			= gcc
CCC			= g++
RANLIB			= ranlib

CPU_ARCH		= $(OS_TEST)
ifeq ($(CPU_ARCH),i386)
CPU_ARCH		= x86
endif
ifeq ($(CPU_ARCH),pc98)
CPU_ARCH		= x86
endif
ifeq ($(CPU_ARCH),amd64)
CPU_ARCH		= x86_64
endif

OS_CFLAGS		= $(DSO_CFLAGS) -Wall -Wno-switch -DFREEBSD -DHAVE_STRERROR -DHAVE_BSD_FLOCK

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared -Wl,-soname -Wl,$(notdir $@)

#
# The default implementation strategy for FreeBSD is pthreads.
#
ifndef CLASSIC_NSPR
USE_PTHREADS		= 1
DEFINES			+= -D_THREAD_SAFE -D_REENTRANT
OS_LIBS			+= -pthread
DSO_LDOPTS		+= -pthread
endif

ARCH			= freebsd

MOZ_OBJFORMAT		:= $(shell test -x /usr/bin/objformat && /usr/bin/objformat || echo elf)

ifeq ($(MOZ_OBJFORMAT),elf)
DLL_SUFFIX		= so
else
DLL_SUFFIX		= so.1.0
endif

MKSHLIB			= $(CC) $(DSO_LDOPTS)
ifdef MAPFILE
	MKSHLIB += -Wl,--version-script,$(MAPFILE)
endif
PROCESS_MAP_FILE = grep -v ';-' $< | \
        sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@

G++INCLUDES		= -I/usr/include/g++

INCLUDES		+= -I/usr/X11R6/include
