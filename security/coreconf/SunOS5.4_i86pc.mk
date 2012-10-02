#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC		= gcc
	OS_CFLAGS	+= -Wall -Wno-format -Wno-switch
	CCC		= g++
	CCC		+= -Wall -Wno-format
	ASFLAGS		+= -x assembler-with-cpp
	OS_CFLAGS	+= $(NOMD_OS_CFLAGS)
	ifdef USE_MDUPDATE
		OS_CFLAGS += -MDupdate $(DEPENDENCIES)
	endif
else
	CC		= cc
	CCC		= CC
	ASFLAGS		+= -Wa,-P
	OS_CFLAGS	+= $(NOMD_OS_CFLAGS)
endif

CPU_ARCH	= x86

MKSHLIB		= $(LD)
MKSHLIB		+= $(DSO_LDOPTS)
NOSUCHFILE	= /solx86-rm-f-sucks
RANLIB		= echo

# for purify
NOMD_OS_CFLAGS	+= -DSVR4 -DSYSV -D_REENTRANT -DSOLARIS -D__svr4__ -Di386

DSO_LDOPTS	+= -G
