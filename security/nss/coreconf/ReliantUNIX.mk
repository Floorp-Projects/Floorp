#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	## gcc-2.7.2 homebrewn
	CC          = gcc
	CCC         = g++
	AS          = $(CC)
	ASFLAGS     += -x assembler-with-cpp
	LD          = gld
	ODD_CFLAGS  = -pipe -Wall -Wno-format -Wno-switch
	ifdef BUILD_OPT
		OPTIMIZER += -O6
	endif
	MKSHLIB     = $(LD)
	MKSHLIB    += -G -h $(@:$(OBJDIR)/%.so=%.so)
	DSO_LDOPTS += -G -Xlinker -Blargedynsym
else
	## native compiler (CDS++ 1.0)
#	CC   = /usr/bin/cc
	CC   = cc
	CCC  = /usr/bin/CC
	AS   = /usr/bin/cc
	ODD_CFLAGS  = 
	ifdef BUILD_OPT
		OPTIMIZER += -O -F Olimit,4000
	endif
	MKSHLIB     = $(CC)
	MKSHLIB    += -G -h $(@:$(OBJDIR)/%.so=%.so)
	DSO_LDOPTS += -G -W l,-Blargedynsym
endif
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@

NOSUCHFILE  = /sni-rm-f-sucks
ODD_CFLAGS += -DSVR4 -DSNI -DRELIANTUNIX
CPU_ARCH    = mips
RANLIB      = /bin/true

# For purify
NOMD_OS_CFLAGS += $(ODD_CFLAGS)

# we do not have -MDupdate ...
OS_CFLAGS   += $(NOMD_OS_CFLAGS)
OS_LIBS     += -lsocket -lnsl -lresolv -lgen -ldl -lc /usr/ucblib/libucb.a

ifdef DSO_BACKEND
	DSO_LDOPTS += -h $(DSO_NAME)
endif
