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
# Config stuff for DEC OSF/1
#

#
# The Bourne shell (sh) on OSF1 doesn't handle "set -e" correctly,
# which we use to stop LOOP_OVER_DIRS submakes as soon as any
# submake fails.  So we use the Korn shell instead.
#
SHELL			= /usr/bin/ksh

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
CLASSIC_NSPR = 1
endif
endif

#
# Prior to OSF1 V4.0, classic nspr is the default (and only) implementation
# strategy.
#
# On OSF1 V4.0, pthreads is the default implementation strategy.
# Classic nspr is also available.
#
ifeq (,$(filter-out V2.0 V3.2,$(OS_RELEASE)))
CLASSIC_NSPR = 1
endif

ifeq ($(CLASSIC_NSPR), 1)
	IMPL_STRATEGY = _EMU
	DEFINES += -D_PR_LOCAL_THREADS_ONLY
else
	USE_PTHREADS = 1
	IMPL_STRATEGY = _PTH
endif

CC			= cc $(NON_LD_FLAGS) -std1
ifneq ($(OS_RELEASE),V2.0)
CC			+= -readonly_strings
endif
# The C++ compiler cxx has -readonly_strings on by default.
CCC			= cxx

RANLIB			= /bin/true

CPU_ARCH		= alpha

ifdef BUILD_OPT
OPTIMIZER		+= -Olimit 4000
endif

NON_LD_FLAGS		= -ieee_with_inexact

OS_CFLAGS		= -DOSF1 -D_REENTRANT

ifneq (,$(filter-out V2.0 V3.2,$(OS_RELEASE)))
OS_CFLAGS		+= -DOSF1_HAVE_MACHINE_BUILTINS_H
endif

ifeq (,$(filter-out V2.0 V3.2,$(OS_RELEASE)))
OS_CFLAGS		+= -DHAVE_INT_LOCALTIME_R
endif

ifeq (V4,$(findstring V4,$(OS_RELEASE)))
OS_CFLAGS		+= -DHAVE_POINTER_LOCALTIME_R
endif

ifeq ($(USE_PTHREADS),1)
OS_CFLAGS		+= -pthread
endif

# The command to build a shared library on OSF1.
MKSHLIB = ld -shared -all -expect_unresolved "*"
DSO_LDOPTS		= -shared
