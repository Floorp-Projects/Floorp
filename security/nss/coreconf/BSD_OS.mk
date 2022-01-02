#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER	= gcc
CC			= gcc
CCC			= g++
RANLIB			= ranlib

ifeq ($(OS_TEST),i386)
	OS_REL_CFLAGS	= -D__i386__
	CPU_ARCH	= x86
else
ifeq ($(OS_TEST),ppc)
	OS_REL_CFLAGS	= -D__ppc__
	CPU_ARCH	= ppc
else
ifeq ($(OS_TEST),sparc)
	OS_REL_CFLAGS	= -D__sparc__
	CPU_ARCH	= sparc
else
# treat the ultrasparc like a regular sparc, at least for now!
ifeq ($(OS_TEST),sparc_v9)
	OS_REL_CFLAGS	= -D__sparc__
	CPU_ARCH	= sparc
endif
endif
endif
endif

DLL_SUFFIX		= so

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -Wno-switch -DBSD_OS -DBSDI -Dunix -DHAVE_STRERROR -DHAVE_BSD_FLOCK

ARCH			= bsdos

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared -Wl,-soname,lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)

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
