#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
# Config stuff for IRIX
#

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
	ODD_CFLAGS	= -Wall -Wno-format
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
ifdef USE_MDUPDATE
	OS_CFLAGS += -MDupdate $(DEPENDENCIES)
endif

ifeq ($(USE_N32),1)
	SHLIB_LD_OPTS	+= -n32 -mips3
endif

MKSHLIB     += $(LD) $(SHLIB_LD_OPTS) -shared -soname $(@:$(OBJDIR)/%.so=%.so)
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $(LIBRARY_NAME).def $@

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
