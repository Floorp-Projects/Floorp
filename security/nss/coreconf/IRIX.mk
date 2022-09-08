#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

#
# The default implementation strategy for Irix is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
	ifeq ($(USE_N32),1)
		IMPL_STRATEGY = _n32_PTH
	else
		IMPL_STRATEGY = _PTH
	endif
endif

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC		= gcc
	AS		= $(CC) -x assembler-with-cpp
	ODD_CFLAGS	= -Wall -Wno-format -Wno-switch
	ifdef BUILD_OPT
		OPTIMIZER	= -O6
	endif
else
	CC	= cc
	CCC		= CC
	ODD_CFLAGS	= -fullwarn -xansi -woff 1209
	ifdef BUILD_OPT
		ifeq ($(USE_N32),1)
			OPTIMIZER	= -O -OPT:Olimit=4000
		else
			OPTIMIZER	= -O -Olimit 4000
		endif
	endif

	# For 6.x machines, include this flag
	ifeq (6., $(findstring 6., $(OS_RELEASE)))
		ifeq ($(USE_N32),1)
			ODD_CFLAGS	+= -n32 -mips3 -exceptions
		else
			ODD_CFLAGS	+= -32 -multigot
		endif
	else
		ODD_CFLAGS		+= -xgot
	endif
	ifeq ($(USE_N32),1)
		OS_CFLAGS	+= -dollar
	endif
endif

ODD_CFLAGS	+= -DSVR4 -DIRIX 

CPU_ARCH	= mips

RANLIB		= /bin/true
# For purify
# NOTE: should always define _SGI_MP_SOURCE
NOMD_OS_CFLAGS += $(ODD_CFLAGS) -D_SGI_MP_SOURCE

OS_CFLAGS += $(NOMD_OS_CFLAGS)

ifeq ($(USE_N32),1)
	SHLIB_LD_OPTS	+= -n32 -mips3
endif

MKSHLIB     += $(LD) $(SHLIB_LD_OPTS) -shared -soname $(@:$(OBJDIR)/%.so=%.so)
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@

DSO_LDOPTS	= -elf -shared -all

ifdef DSO_BACKEND
	DSO_LDOPTS += -soname $(DSO_NAME)
endif

#
# Revision notes:
#
# In the IRIX compilers prior to version 7.2, -n32 implied -mips3.
# Beginning in the 7.2 compilers, -n32 implies -mips4 when the compiler
# is running on a system with a mips4 CPU (e.g. R8K, R10K).
# We want our code to explicitly be mips3 code, so we now explicitly
# set -mips3 whenever we set -n32.
#
