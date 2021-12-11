#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

###
NS_USE_NATIVE = 1

# NS_USE_GCC = 1

export PATH:=$(PATH):/opt/ncc/bin
###

RANLIB           = true
GCC_FLAGS_EXTRA += -pipe

DEFINES		+= -DSVR4 -DSYSV -DHAVE_STRERROR -DNCR

OS_CFLAGS	+= -Hnocopyr -DSVR4 -DSYSV -DHAVE_STRERROR -DNCR -DPRFSTREAMS_BROKEN

ifdef NS_USE_NATIVE
	CC       = cc
	CCC      = ncc
	CXX      = ncc
#	OS_LIBS += -L/opt/ncc/lib 
else
#	OS_LIBS	+=
endif

#OS_LIBS    += -lsocket -lnsl -ldl -lc

MKSHLIB     += $(LD) $(DSO_LDOPTS)
#DSO_LDOPTS += -G -z defs
DSO_LDOPTS += -G
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@

CPU_ARCH    = x86
ARCH        = ncr

NOSUCHFILE  = /solaris-rm-f-sucks

# now take care of default GCC (rus@5/5/97)
 
ifdef NS_USE_GCC
	# if gcc-settings are redefined already - don't touch it
	#
	ifeq (,$(findstring gcc, $(CC)))
		CC   = gcc
		CCC  = g++
		CXX  = g++
		# always use -fPIC - some makefiles are still broken and don't distinguish
		# situation when they build shared and static libraries
		CFLAGS  += -fPIC -Wall -Wno-switch $(GCC_FLAGS_EXTRA)
#		OS_LIBS += -L/usr/local/lib -lstdc++ -lg++ -lgcc
	endif
endif
###
