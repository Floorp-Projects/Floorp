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
# Config stuff for SunOS4.1
#

include $(MOD_DEPTH)/config/UNIX.mk

# SunOS 4 _requires_ that shared libs have a version number.
# XXX FIXME: Version number should use NSPR_VERSION_NUMBER?
DLL_SUFFIX	= so.1.0

CC			= gcc
CCC			= g++
COMPILER_TAG		= _gcc

RANLIB			= ranlib

CPU_ARCH		= sparc

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS		= -Wall -Wno-format -DSUNOS4

ifdef NO_MDUPDATE
OS_CFLAGS		= $(DSO_CFLAGS) $(NOMD_OS_CFLAGS)
else
OS_CFLAGS		= $(DSO_CFLAGS) $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
endif

MKSHLIB			= $(LD) $(DSO_LDOPTS)

HAVE_PURIFY		= 1

NOSUCHFILE		= /no-such-file

DSO_LDOPTS		=
# -fPIC generates position-independent code for use in a shared library.
DSO_CFLAGS		= -fPIC
