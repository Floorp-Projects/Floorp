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
# Config stuff for SNI SINIX (aka ReliantUNIX)
#

include $(MOD_DEPTH)/config/UNIX.mk

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		= x86
else
CPU_ARCH		= mips
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

# use gcc -tf-
NS_USE_GCC		= 1

ifeq ($(NS_USE_GCC),1)
## gcc-2.7.2 homebrewn
CC			= gcc
COMPILER_TAG		= _gcc
CCC			= g++
AS			= $(CC) -x assembler-with-cpp
ifeq ($(CPU_ARCH),mips)
LD			= gld
endif
ODD_CFLAGS		= -Wall -Wno-format
ifeq ($(CPU_ARCH),mips)
# The -pipe flag only seems to work on the mips version of SINIX.
ODD_CFLAGS		+= -pipe
endif
ifdef BUILD_OPT
OPTIMIZER		= -O
#OPTIMIZER		= -O6
endif
MKSHLIB			= $(LD) -G -z defs -h $(@:$(OBJDIR)/%.so=%.so)
#DSO_LDOPTS		= -G -Xlinker -Blargedynsym
else
## native compiler (CDS++ 1.0)
CC			= /usr/bin/cc
CCC			= /usr/bin/CC
AS			= /usr/bin/cc
#ODD_CFLAGS		= -fullwarn -xansi
ODD_CFLAGS		= 
ifdef BUILD_OPT
#OPTIMIZER		= -Olimit 4000
OPTIMIZER		= -O -F Olimit,4000
endif
MKSHLIB			= $(LD) -G -z defs -h $(@:$(OBJDIR)/%.so=%.so)
#DSO_LDOPTS		= -G -W l,-Blargedynsym
endif

ifeq ($(CPU_ARCH),x86)
DEFINES			+= -Di386
endif

ODD_CFLAGS		+= -DSVR4 -DSNI -DRELIANTUNIX -Dsinix -DHAVE_SVID_GETTOD

# On SINIX 5.43, need to define IP_MULTICAST in order to get the
# IP multicast macro and struct definitions in netinet/in.h.
# (SINIX 5.42 does not have IP multicast at all.)
ifeq ($(OS_RELEASE),5.43)
ODD_CFLAGS		+= -DIP_MULTICAST
endif

RANLIB			= /bin/true

# For purify
NOMD_OS_CFLAGS		= $(ODD_CFLAGS)

# we do not have -MDupdate ...
OS_CFLAGS		= $(NOMD_OS_CFLAGS)
OS_LIBS			= -lsocket -lnsl -lresolv -ldl -lc
NOSUCHFILE		= /no-such-file

HAVE_PURIFY		= 0

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
