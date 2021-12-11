#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Config stuff for SCO Unixware 2.1
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = $(CORE_DEPTH)/build/hcc

CC         = $(CORE_DEPTH)/build/hcc
CCC         = $(CORE_DEPTH)/build/hcpp
RANLIB      = true
OS_CFLAGS   = -KPIC -DSVR4 -DSYSV -DUNIXWARE
MKSHLIB     = $(LD)
MKSHLIB    += $(DSO_LDOPTS)
DSO_LDOPTS += -G
CPU_ARCH    = x86
ARCH        = sco
NOSUCHFILE  = /solaris-rm-f-sucks
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@

