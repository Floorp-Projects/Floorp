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
# Config stuff for OpenBSD 
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

ifndef CLASSIC_NSPR
USE_PTHREADS		= 1
DEFINES			+= -pthread
OS_LIBS			+= -pthread
DSO_LDOPTS		+= -pthread
endif

DLL_SUFFIX		= so.1.0

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DOPENBSD

OS_LIBS			= 

ARCH			= openbsd

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared -Wl,-soname,lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
DSO_LDFLAGS		=

MKSHLIB			= $(CC) $(DSO_LDOPTS)

