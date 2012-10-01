#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = $(CORE_DEPTH)/build/hcc

CPU_ARCH		= mips

ifdef NS_USE_GCC
CC			= gcc
CCC			= g++
else
CC			= $(CORE_DEPTH)/build/hcc
OS_CFLAGS		= -Xa -KGnum=0 -KOlimit=4000
CCC			= g++
endif

MKSHLIB			= $(LD) $(DSO_LDOPTS)
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@

RANLIB			= /bin/true

OS_CFLAGS		+= $(ODD_CFLAGS) -DSVR4 -D__SVR4 -DNEC -Dnec_ews -DHAVE_STRERROR
OS_LIBS			= -lsocket -lnsl -ldl $(LDOPTIONS)
LDOPTIONS		= -lc -L/usr/ucblib -lucb

NOSUCHFILE		= /nec-rm-f-sucks

DSO_LDOPTS		= -G
