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
# Config stuff for IRIX
#

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
CLASSIC_NSPR = 1
endif
endif

#
# On IRIX 5.x, classic nspr (user-level threads on top of sprocs)
# is the default (and only) implementation strategy.
#
# On IRIX 6.x and later, the default implementation strategy is
# pthreads.  Classic nspr is also available.
#
ifeq ($(basename $(OS_RELEASE)),5)
CLASSIC_NSPR = 1
endif

ifeq ($(CLASSIC_NSPR),1)
	IMPL_STRATEGY = _MxN
else
	USE_PTHREADS = 1
	USE_N32 = 1
	IMPL_STRATEGY = _PTH
endif

ifdef NS_USE_GCC
	CC			= gcc
	COMPILER_TAG		= _gcc
	AS			= $(CC) -x assembler-with-cpp -D_ASM -mips2
	ODD_CFLAGS		= -Wall -Wno-format
	ifdef BUILD_OPT
		OPTIMIZER		= -O6
	endif
else
	CC			= cc
	CCC         = CC
	ODD_CFLAGS		= -fullwarn -xansi
	ifdef BUILD_OPT
		ifneq ($(USE_N32),1)
			OPTIMIZER		= -O -Olimit 4000
		else
			OPTIMIZER		= -O -OPT:Olimit=4000
		endif
	endif

#
# The default behavior is still -o32 generation, hence the explicit tests
# for -n32 and -64 and implicitly assuming -o32. If that changes, ...
#
	ifeq ($(basename $(OS_RELEASE)),6)
		ODD_CFLAGS += -multigot
		SHLIB_LD_OPTS = -no_unresolved
		ifeq ($(USE_N32),1)
			ODD_CFLAGS += -n32 -woff 1209
			COMPILER_TAG = _n32
			LDOPTS += -n32
        	SHLIB_LD_OPTS += -n32
			ifeq ($(OS_RELEASE), 6_2)
				LDOPTS += -Wl,-woff,85
				SHLIB_LD_OPTS += -woff 85
			endif
		else
			ifeq ($(USE_64),1)
				ODD_CFLAGS +=  -64
				COMPILER_TAG = _64
			else
				ODD_CFLAGS +=  -32
				COMPILER_TAG = _o32
			endif
		endif
	else
		ODD_CFLAGS += -xgot
	endif
endif

ODD_CFLAGS		+= -DSVR4 -DIRIX

CPU_ARCH		= mips

RANLIB			= /bin/true

# For purify
# XXX: should always define _SGI_MP_SOURCE
NOMD_OS_CFLAGS		= $(ODD_CFLAGS) -D_SGI_MP_SOURCE

ifeq ($(OS_RELEASE),5.3)
OS_CFLAGS		+= -DIRIX5_3
endif

ifneq ($(basename $(OS_RELEASE)),5)
OS_CFLAGS		+= -D_PR_HAVE_SGI_PRDA_PROCMASK
endif

ifeq (,$(filter-out 6.5,$(OS_RELEASE)))
OS_CFLAGS		+= -D_PR_HAVE_GETPROTO_R -D_PR_HAVE_GETPROTO_R_POINTER
endif

ifndef NO_MDUPDATE
OS_CFLAGS		+= $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
else
OS_CFLAGS		+= $(NOMD_OS_CFLAGS)
endif

# -rdata_shared is an ld option that puts string constants and
# const data into the text segment, where they will be shared
# across processes and be read-only.
MKSHLIB			= $(LD) $(SHLIB_LD_OPTS) -rdata_shared -shared -soname $(@:$(OBJDIR)/%.so=%.so)

HAVE_PURIFY		= 1

DSO_LDOPTS		= -elf -shared -all

ifdef DSO_BACKEND
DSO_LDOPTS		+= -soname $(DSO_NAME)
endif
