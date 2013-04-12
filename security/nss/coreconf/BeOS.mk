#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

XP_DEFINE := $(XP_DEFINE:-DXP_UNIX=-DXP_BEOS)

USE_PTHREADS = 

ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
endif

CC			= gcc
CCC			= g++
RANLIB			= ranlib

DEFAULT_COMPILER = gcc

ifeq ($(OS_TEST),ppc)
	OS_REL_CFLAGS	= -Dppc
	CPU_ARCH	= ppc
else
	OS_REL_CFLAGS	= -Di386
	CPU_ARCH	= x86
endif

MKSHLIB		= $(CC) -nostart -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)
ifdef BUILD_OPT
	OPTIMIZER	= -O2
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -Wno-switch -pipe
OS_LIBS			= -lbe

DEFINES			+= -DBEOS

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT
endif

ARCH			= beos

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		=
