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
# Config stuff for NEC Mips SYSV
#

include $(MOD_DEPTH)/config/UNIX.mk

CPU_ARCH		= mips

ifdef NS_USE_GCC
CC			= gcc
CCC			= g++
else
CC			= $(NSDEPTH)/build/hcc cc -Xa -KGnum=0 -KOlimit=4000
CCC			= g++
endif

MKSHLIB			= $(LD) $(DSO_LDOPTS)

RANLIB			= /bin/true

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
OS_CFLAGS		= $(ODD_CFLAGS) -DSVR4 -D__SVR4 -DNEC -Dnec_ews -DHAVE_STRERROR
OS_LIBS			= -lsocket -lnsl -ldl $(LDOPTIONS)
LDOPTIONS		= -lc -L/usr/ucblib -lucb

NOSUCHFILE		= /no-such-file

DSO_LDOPTS		= -G
