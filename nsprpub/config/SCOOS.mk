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
# Config stuff for SCO OpenServer for x86.
# SCO OpenServer 5, based on SVR3.2, is intended for small to
# medium customers.
#

include $(MOD_DEPTH)/config/UNIX.mk

CC			= cc -b elf -KPIC
CCC			= g++ -b elf -I/usr/local/lib/g++-include
# CCC			= $(DEPTH)/build/hcpp +.cpp +w
RANLIB			= /bin/true

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
#
# -DSCO - Changes to Netscape source (consistent with AIX, LINUX, etc..)
# -Dsco - Needed for /usr/include/X11/*
#
OS_CFLAGS		= -DSYSV -D_SVID3 -DHAVE_STRERROR -D_PR_NEED_H_ERRNO -DSCO -Dsco
#OS_LIBS			= -lpmapi -lsocket -lc

MKSHLIB			= $(LD) $(DSO_LDOPTS)

XINC			= /usr/include/X11
MOTIFLIB		= -lXm
INCLUDES		+= -I$(XINC)

CPU_ARCH		= x86
GFX_ARCH		= x
ARCH			= sco

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/sco.lm
EN_LOCALE		= C
DE_LOCALE		= de_DE.ISO8859-1
FR_LOCALE		= fr_FR.ISO8859-1
JP_LOCALE		= ja
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko_KR.EUC
CN_LOCALE		= zh
TW_LOCALE		= zh
I2_LOCALE		= i2

LOC_LIB_DIR		= /usr/lib/X11

NOSUCHFILE		= /no-such-file

BSDECHO			= /bin/echo

#
# These defines are for building unix plugins
#
BUILD_UNIX_PLUGINS	= 1
#DSO_LDOPTS		= -b elf -G -z defs
DSO_LDOPTS		= -b elf -G
DSO_LDFLAGS		= -nostdlib -L/lib -L/usr/lib -lXm -lXt -lX11 -lgen

# Used for Java compiler
EXPORT_FLAGS = -W l,-Bexport
