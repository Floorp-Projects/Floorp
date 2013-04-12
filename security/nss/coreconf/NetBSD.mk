#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER	= gcc
CC			= gcc
CCC			= g++
RANLIB			= ranlib

CPU_ARCH		:= $(shell uname -p)
ifeq ($(CPU_ARCH),i386)
OS_REL_CFLAGS		= -Di386
CPU_ARCH		= x86
endif

ifndef OBJECT_FMT
OBJECT_FMT		:= $(shell if echo __ELF__ | $${CC:-cc} -E - | grep -q __ELF__ ; then echo a.out ; else echo ELF ; fi)
endif

ifeq ($(OBJECT_FMT),ELF)
DLL_SUFFIX		= so
else
DLL_SUFFIX		= so.1.0
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -Wno-switch -pipe -DNETBSD -Dunix -DHAVE_STRERROR -DHAVE_BSD_FLOCK

OS_LIBS			= -lcompat

ARCH			= netbsd

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared
ifeq ($(OBJECT_FMT),ELF)
DSO_LDOPTS		+= -Wl,-soname,lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
endif

ifdef LIBRUNPATH
DSO_LDOPTS		+= -Wl,-R$(LIBRUNPATH)
endif

MKSHLIB			= $(CC) $(DSO_LDOPTS)
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@


G++INCLUDES		= -I/usr/include/g++

INCLUDES		+= -I/usr/X11R6/include
