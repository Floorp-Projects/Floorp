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
# Config stuff for OpenBSD
#

include $(MOD_DEPTH)/config/UNIX.mk

CC                     = gcc
CCC                    = g++
RANLIB                 = ranlib

OS_REL_CFLAGS          =
ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH               = x86
else
CPU_ARCH               = $(OS_TEST)
endif

OS_CFLAGS              = $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DOPENBSD -DHAVE_STRERROR -DHAVE_BSD_FLOCK

ifeq ($(USE_PTHREADS),1)
OS_LIBS                        = -lc_r
# XXX probably should define _THREAD_SAFE too.
else
OS_LIBS                        = -lc
DEFINES                        += -D_PR_LOCAL_THREADS_ONLY
endif

ARCH                   = openbsd

DSO_CFLAGS             = -fPIC
DSO_LDFLAGS            =
DSO_LDOPTS             = -Bshareable
ifeq ($(OS_TEST),alpha)
DSO_LDOPTS             = -shared
endif
ifeq ($(OS_TEST),mips)
DSO_LDOPTS             = -shared
endif
ifeq ($(OS_TEST),pmax)  
DSO_LDOPTS             = -shared
endif

MKSHLIB                        = $(LD) $(DSO_LDOPTS)

G++INCLUDES            = -I/usr/include/g++
