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
# Config stuff for NCR SVR4 MP-RAS
#

include $(MOD_DEPTH)/config/UNIX.mk
###
NS_USE_NATIVE = 1

# NS_USE_GCC = 1

export PATH:=$(PATH):/opt/ncc/bin
###

RANLIB          = true
GCC_FLAGS_EXTRA = -pipe

DEFINES		+= -DSVR4 -DSYSV -DHAVE_STRERROR -DNCR -D_PR_LOCAL_THREADS_ONLY

ifeq (,$(filter-out 2.03,$(OS_RELEASE)))
DEFINES		+= -D_PR_STAT_HAS_ST_ATIM
else
DEFINES		+= -D_PR_STAT_HAS_ST_ATIM_UNION
endif

ifdef NS_USE_NATIVE
CC              = cc
CCC             = ncc
OS_CFLAGS	= -Hnocopyr
#OS_LIBS         = -L/opt/ncc/lib 
else
#OS_LIBS		=
endif

CCC	= g++

#OS_LIBS	       += -lsocket -lnsl -ldl -lc

MKSHLIB		= $(LD) $(DSO_LDOPTS)
#DSO_LDOPTS	= -G -z defs
DSO_LDOPTS	= -G

CPU_ARCH	= x86
ARCH		= ncr

NOSUCHFILE	= /no-such-file

# now take care of default GCC (rus@5/5/97)
 
ifdef NS_USE_GCC
# if gcc-settings are redefined already - don't touch it
#
ifeq (,$(findstring gcc, $(CC)))
CC      = gcc
CCC     = g++
CXX     = g++
COMPILER_TAG = _gcc
# always use -fPIC - some makefiles are still broken and don't distinguish
# situation when they build shared and static libraries
CFLAGS  += -fPIC -Wall $(GCC_FLAGS_EXTRA)
#OS_LIBS += -L/usr/local/lib -lstdc++ -lg++ -lgcc
endif
endif
###

