#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
# 
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

#
# Config stuff for NEXTSTEP
#

include $(MOD_DEPTH)/config/UNIX.mk

CC			= cc
CCC			= cc++

RANLIB			= ranlib

OS_REL_CFLAGS		= -D$(shell uname -p)
CPU_ARCH		:= $(shell uname -p)

# "Commons" are tentative definitions in a global scope, like this:
#     int x;
# The meaning of a common is ambiguous.  It may be a true definition:
#     int x = 0;
# or it may be a declaration of a symbol defined in another file:
#     extern int x;
# Use the -fno-common option to force all commons to become true
# definitions so that the linker can catch multiply-defined symbols.
# Also, common symbols are not allowed with Rhapsody dynamic libraries.

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -fno-common -pipe -DNEXTSTEP -DHAVE_STRERROR -DHAVE_BSD_FLOCK -D_POSIX_SOURCE -traditional-cpp -posix

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY

ARCH			= $(CPU_ARCH)

# May override this with -bundle to create a loadable module.
#DSO_LDOPTS		= -dynamiclib

#MKSHLIB			= $(CC) -arch $(CPU_ARCH) $(DSO_LDOPTS)
DLL_SUFFIX		= dylib
