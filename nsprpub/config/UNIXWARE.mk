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
# Config stuff for SCO UnixWare
# UnixWare is intended for high-end enterprise customers.
# UnixWare 2.1 and 2.1.1 are based on SVR4.  (2.1.2 is a maintenance
# release.)
# UnixWare 7 (codename Gemini) is based on what SCO calls SVR5.
# The somewhat odd version number 7 was chosen to suggest that
#     UnixWare 2 + OpenServer 5 = UnixWare 7
#

include $(MOD_DEPTH)/config/UNIX.mk

ifeq (,$(filter-out 2.1,$(OS_RELEASE)))
CC		= $(NSDEPTH)/build/hcc cc
CCC		= $(NSDEPTH)/build/hcpp CC
else
CC		= cc
CCC		= CC
endif

RANLIB		= true

DEFINES		+= -D_PR_LOCAL_THREADS_ONLY
OS_CFLAGS	= -DSVR4 -DSYSV -DUNIXWARE

ifeq (,$(filter-out 2.1,$(OS_RELEASE)))
OS_CFLAGS	+= -D_PR_NO_LARGE_FILES
else
OS_CFLAGS	+= -D_LARGEFILE64_SOURCE -D_PR_HAVE_OFF64_T -D_PR_HAVE_SOCKADDR_LEN
endif

MKSHLIB		= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS	= -G

CPU_ARCH	= x86
ARCH		= sco

NOSUCHFILE	= /no-such-file
