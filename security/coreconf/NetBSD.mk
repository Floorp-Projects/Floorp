#
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
# Config stuff for NetBSD
#

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

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DNETBSD -Dunix -DHAVE_STRERROR -DHAVE_BSD_FLOCK

OS_LIBS			= -lcompat

ARCH			= netbsd

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=
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
PROCESS_MAP_FILE = cp $(LIBRARY_NAME).def $@


G++INCLUDES		= -I/usr/include/g++

INCLUDES		+= -I/usr/X11R6/include
