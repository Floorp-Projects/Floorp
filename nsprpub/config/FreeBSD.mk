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
# Config stuff for FreeBSD
#

include $(MOD_DEPTH)/config/UNIX.mk

CC			= gcc
CCC			= g++
RANLIB			= ranlib

ifeq ($(OS_TEST),alpha)
CPU_ARCH		= alpha
else
OS_REL_CFLAGS		= -mno-486 -Di386
CPU_ARCH		= x86
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DFREEBSD -DHAVE_STRERROR -DHAVE_BSD_FLOCK

ifeq ($(USE_PTHREADS),1)
IMPL_STRATEGY		= _PTH
OS_LIBS			= -lc_r
DEFINES			+= -D_THREAD_SAFE
OS_CFLAGS		+= -pthread
else
IMPL_STRATEGY		= _EMU
OS_LIBS			= -lc
DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
endif

ARCH			= freebsd

MOZ_OBJFORMAT          := $(shell test -x /usr/bin/objformat && /usr/bin/objformat || echo aout)

ifeq ($(MOZ_OBJFORMAT),elf)
DLL_SUFFIX		= so
else
DLL_SUFFIX		= so.1.0
endif

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -Bshareable
DSO_LDFLAGS		=

MKSHLIB			= $(LD) $(DSO_LDOPTS)

G++INCLUDES		= -I/usr/include/g++
