#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
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
CLASSIC_NSPR = 1
endif

#
# On OSF1 V3.2, classic nspr is the default (and only) implementation
# strategy.
#
# On OSF1 V4.0, pthreads is the default implementation strategy.
# Classic nspr is also available.
#
ifneq ($(OS_RELEASE),V3.2)
USE_PTHREADS = 1
ifeq ($(CLASSIC_NSPR), 1)
	USE_PTHREADS =
	IMPL_STRATEGY := _CLASSIC
	DEFINES += -D_PR_LOCAL_THREADS_ONLY
endif
endif

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
IMPL_STRATEGY =
endif

CC			= cc $(NON_LD_FLAGS) -std -readonly_strings
# The C++ compiler cxx has -readonly_strings on by default.
CCC			= cxx

RANLIB			= /bin/true

CPU_ARCH		= alpha

ifdef BUILD_OPT
OPTIMIZER		+= -Olimit 4000
endif

NON_LD_FLAGS		= -ieee_with_inexact

OS_CFLAGS		= -DOSF1 -D_REENTRANT -taso

ifeq ($(OS_RELEASE),V3.2)
OS_CFLAGS		+= -DOSF1V3
endif

ifeq ($(OS_RELEASE),V4.0)
OS_CFLAGS		+= -DOSF1V4
endif

ifeq ($(USE_PTHREADS),1)
OS_CFLAGS		+= -pthread
endif

# The command to build a shared library on OSF1.
MKSHLIB = ld -shared -all -expect_unresolved "*"
DSO_LDOPTS		= -shared
