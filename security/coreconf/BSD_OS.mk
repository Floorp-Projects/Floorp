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
#   Kurt J. Lidl
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

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -DBSD_OS -DBSDI -Dunix -DHAVE_STRERROR -DHAVE_BSD_FLOCK

ARCH			= bsdos

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=
DSO_LDOPTS		+= -Wl,-soname,lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)

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
