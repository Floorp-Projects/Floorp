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
# Config stuff for BSD/OS Unix.
#

include $(MOD_DEPTH)/config/UNIX.mk

ifeq (,$(filter-out 1.1 4.0 4.0.1,$(OS_RELEASE)))
CC		= gcc -Wall -Wno-format
CCC		= g++
else
CC		= shlicc2
CCC		= shlicc2
endif
RANLIB		= ranlib

ifeq ($(USE_PTHREADS),1)
IMPL_STRATEGY = _PTH
DEFINES		+= -D_PR_NEED_PTHREAD_INIT
else
IMPL_STRATEGY = _EMU
DEFINES		+= -D_PR_LOCAL_THREADS_ONLY
endif

OS_CFLAGS	= $(DSO_CFLAGS) -DBSDI -DHAVE_STRERROR -DNEED_BSDREGEX

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH	= x86
endif
ifeq (sparc,$(findstring sparc,$(OS_TEST)))
CPU_ARCH	= sparc
endif

ifeq ($(OS_RELEASE),2.1)
OS_CFLAGS	+= -D_PR_TIMESPEC_HAS_TS_SEC
endif

ifeq (,$(filter-out 1.1 2.1,$(OS_RELEASE)))
OS_CFLAGS	+= -D_PR_BSDI_JMPBUF_IS_ARRAY
else
OS_CFLAGS	+= -D_PR_SELECT_CONST_TIMEVAL -D_PR_BSDI_JMPBUF_IS_STRUCT
endif

NOSUCHFILE	= /no-such-file

ifeq ($(OS_RELEASE),1.1)
OS_CFLAGS	+= -D_PR_STAT_HAS_ONLY_ST_ATIME -D_PR_NEED_H_ERRNO
else
OS_CFLAGS	+= -DHAVE_DLL -DUSE_DLFCN -D_PR_STAT_HAS_ST_ATIMESPEC
OS_LIBS		= -ldl
ifeq (,$(filter-out 4.0 4.0.1,$(OS_RELEASE)))
MKSHLIB		= $(CC) $(DSO_LDOPTS)
DSO_CFLAGS	= -fPIC
DSO_LDOPTS	= -shared -Wl,-soname,$(@:$(OBJDIR)/%.so=%.so)
else
MKSHLIB		= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS	= -r
endif
endif
