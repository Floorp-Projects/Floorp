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

######################################################################
# Config stuff for Linux (all architectures)
######################################################################

######################################################################
# Version-independent
######################################################################

include $(MOD_DEPTH)/config/UNIX.mk

#
# The default implementation strategy for Linux is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
IMPL_STRATEGY = _PTH
DEFINES			+= -D_REENTRANT
else
DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
endif

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		:= x86
else
CPU_ARCH		:= $(OS_TEST)
endif

CC			= gcc
CCC			= g++
RANLIB			= ranlib

OS_INCLUDES		=
G++INCLUDES		= -I/usr/include/g++

# The -pipe flag doesn't work on Alpha Linux in recursive sub-makes
#PLATFORM_FLAGS		= -ansi -Wall -pipe -DLINUX -Dlinux
PLATFORM_FLAGS		= -ansi -Wall -DLINUX -Dlinux
PORT_FLAGS		= -D_POSIX_SOURCE -D_BSD_SOURCE -DHAVE_STRERROR

OS_CFLAGS		= $(DSO_CFLAGS) $(PLATFORM_FLAGS) $(PORT_FLAGS)

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(CPU_ARCH),alpha)
PLATFORM_FLAGS		+= -DLINUX1_2 -D_ALPHA_ -D__alpha
endif
ifeq ($(CPU_ARCH),ppc)
PLATFORM_FLAGS		+= -DMKLINUX -DMACLINUX -DLINUX1_2
OS_INCLUDES		+= -I/usr/local/include
endif
ifeq ($(CPU_ARCH),sparc)
PLATFORM_FLAGS		+= -DLINUX1_2
endif
ifeq ($(CPU_ARCH),x86)
PLATFORM_FLAGS		+= -mno-486 -DLINUX1_2 -Di386
endif
ifeq ($(CPU_ARCH),m68k)
#
# gcc on Linux/m68k either has a bug or triggers a code-sequence
# bug in the 68060 which causes gcc to crash.  The simplest way to
# avoid this is to enable a minimum level of optimization.
#
ifndef BUILD_OPT
OPTIMIZER		+= -O
endif
PLATFORM_FLAGS		+= -m68020-40 -DLINUX1_2
endif

ifeq ($(OS_RELEASE),2.0)
PLATFORM_FLAGS		+= -DLINUX2_0
endif

#
# Linux ppc and 2.0 have shared libraries.
#

MKSHLIB			= $(LD) $(DSO_LDOPTS) -soname $(@:$(OBJDIR)/%.so=%.so)

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

ifneq (,$(filter-out x86 ppc,$(CPU_ARCH)))
CPU_ARCH_TAG		= _$(CPU_ARCH)
endif

######################################################################
# Other
######################################################################

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=
