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
# Config stuff for Data General DG/UX
#
# Initial DG/UX port by Marc Fraioli <fraioli@dg-rtp.dg.com>
#

include $(MOD_DEPTH)/config/UNIX.mk

CC		= gcc
CCC		= g++

RANLIB		= true

DEFINES		+= -D_PR_LOCAL_THREADS_ONLY 
OS_CFLAGS	= -DSVR4 -DSYSV -DDGUX -D_DGUX_SOURCE -D_POSIX4A_DRAFT6_SOURCE 

MKSHLIB		= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS	= -G 

CPU_ARCH	= x86
ARCH		= dgux

NOSUCHFILE	= /no-such-file

ifdef BUILD_OPT
OPTIMIZER	= -O2
else
# -g would produce a huge executable.
OPTIMIZER	=
endif
