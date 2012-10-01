#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# The Bourne shell (sh) on OSF1 doesn't handle "set -e" correctly,
# which we use to stop LOOP_OVER_DIRS submakes as soon as any
# submake fails.  So we use the Korn shell instead.
#
SHELL = /usr/bin/ksh

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

CC         = cc
OS_CFLAGS += $(NON_LD_FLAGS) -std1
CCC        = cxx
RANLIB     = /bin/true
CPU_ARCH   = alpha

ifdef BUILD_OPT
	OPTIMIZER += -Olimit 4000
endif

NON_LD_FLAGS += -ieee_with_inexact
OS_CFLAGS    += -DOSF1 -D_REENTRANT 

ifeq ($(USE_PTHREADS),1)
	OS_CFLAGS += -pthread
endif

# The command to build a shared library on OSF1.
MKSHLIB    += ld -shared -expect_unresolved "*" -soname $(notdir $@)
ifdef MAPFILE
MKSHLIB += -hidden -input $(MAPFILE)
endif
PROCESS_MAP_FILE = grep -v ';+' $< | grep -v ';-' | \
 sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,^,-exported_symbol ,' > $@

DSO_LDOPTS += -shared

# required for freebl
USE_64=1
# this platform name does not use a bit tag due to only having a 64-bit ABI
64BIT_TAG=

