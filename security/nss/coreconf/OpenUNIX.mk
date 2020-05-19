#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = gcc

CC         = gcc
OS_CFLAGS += -fPIC
CCC        = g++
CCC       += -DPRFSTREAMS_BROKEN -I/usr/gnu/lib/g++-include
# CCC      = $(CORE_DEPTH)/build/hcpp
# CCC     += +.cpp +w
RANLIB     = /bin/true

#
# -DSCO_PM - Policy Manager AKA: SCO Licensing
# -DSCO - Changes to Netscape source (consistent with AIX, LINUX, etc..)
# -Dsco - Needed for /usr/include/X11/*
#
OS_CFLAGS   += -DSCO_SV -DSYSV -D_SVID3 -DHAVE_STRERROR -DSW_THREADS -DSCO_PM -DSCO -Dsco
#OS_LIBS     += -lpmapi -lsocket -lc
MKSHLIB      = $(LD)
MKSHLIB     += $(DSO_LDOPTS)
XINC         = /usr/include/X11
MOTIFLIB    += -lXm
INCLUDES    += -I$(XINC)
CPU_ARCH     = x86
GFX_ARCH     = x
ARCH         = sco
LOCALE_MAP   = $(CORE_DEPTH)/cmd/xfe/intl/sco.lm
EN_LOCALE    = C
DE_LOCALE    = de_DE.ISO8859-1
FR_LOCALE    = fr_FR.ISO8859-1
JP_LOCALE    = ja
SJIS_LOCALE  = ja_JP.SJIS
KR_LOCALE    = ko_KR.EUC
CN_LOCALE    = zh
TW_LOCALE    = zh
I2_LOCALE    = i2
LOC_LIB_DIR  = /usr/lib/X11
NOSUCHFILE   = /solaris-rm-f-sucks
BSDECHO      = /bin/echo
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@

#
# These defines are for building unix plugins
#
BUILD_UNIX_PLUGINS  = 1
#DSO_LDOPTS         += -b elf -G -z defs
DSO_LDOPTS         += -G
