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
# Config stuff for BSDI Unix for x86.
#

include $(MOD_DEPTH)/config/UNIX.mk

#CC		= gcc -Wall -Wno-format
#CCC		= g++
CC		= shlicc2
CCC		= shlicc2
RANLIB		= ranlib

DEFINES		+= -D_PR_LOCAL_THREADS_ONLY
OS_CFLAGS	= -DBSDI -DHAVE_STRERROR -D__386BSD__ -DNEED_BSDREGEX -Di386
OS_LIBS		= -lcompat -ldl

ifeq ($(OS_RELEASE),2.1)
OS_CFLAGS	+= -DBSDI_2
endif

G++INCLUDES	= -I/usr/include/g++

CPU_ARCH	= x86

NOSUCHFILE	= /no-such-file

MKSHLIB		= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS	= -r
