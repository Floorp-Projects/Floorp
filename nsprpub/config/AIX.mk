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
# Config stuff for AIX.
#

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
CLASSIC_NSPR = 1
endif

#
# There are three implementation strategies available on AIX:
# pthreads, classic, and pthreads-user.  The default is pthreads.
# 
ifeq ($(CLASSIC_NSPR),1)
	PTHREADS_USER =
	USE_PTHREADS =
	IMPL_STRATEGY = _CLASSIC
	DEFINES += -D_PR_LOCAL_THREADS_ONLY
else
ifeq ($(PTHREADS_USER),1)
	USE_PTHREADS =
	IMPL_STRATEGY = _PTH_USER
else
	USE_PTHREADS = 1
endif
endif

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
IMPL_STRATEGY =
endif

ifeq ($(CLASSIC_NSPR),1)
CC		= xlC
CCC		= xlC
else
CC		= xlC_r
CCC		= xlC_r
endif

CPU_ARCH	= rs6000

RANLIB		= ranlib

OS_CFLAGS 	= -qro -qroconst -DAIX -DSYSV
ifeq ($(CC),xlC_r)
OS_CFLAGS 	+= -qarch=com
endif

ifeq ($(OS_RELEASE),4.1)
OS_CFLAGS	+= -DAIX4_1
else
DSO_LDOPTS	= -brtl -bM:SRE -bnoentry -bexpall
MKSHLIB		= $(LD) $(DSO_LDOPTS)
ifeq ($(OS_RELEASE),4.3)
OS_CFLAGS	+= -DAIX4_3
endif
endif

#
# Special link info for constructing AIX programs. On AIX we have to
# statically link programs that use NSPR into a single .o, rewriting the
# calls to select to call "aix". Once that is done we then can
# link that .o with a .o built in nspr which implements the system call.
#
ifneq ($(OS_RELEASE),4.1)
AIX_LINK_OPTS	= -brtl -bnso -berok
else
AIX_LINK_OPTS	= -bnso -berok
#AIX_LINK_OPTS	= -bnso -berok -brename:.select,.wrap_select -brename:.poll,.wrap_poll -bI:/usr/lib/syscalls.exp
endif

AIX_WRAP	= $(DIST)/lib/aixwrap.o
AIX_TMP		= $(OBJDIR)/_aix_tmp.o
