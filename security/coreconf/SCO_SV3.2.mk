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
# Config stuff for SCO Unix for x86.
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

CC         = cc
OS_CFLAGS += -b elf -KPIC
CCC        = g++
CCC       += -b elf -DPRFSTREAMS_BROKEN -I/usr/local/lib/g++-include
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
PROCESS_MAP_FILE = cp $(LIBRARY_NAME).def $@

#
# These defines are for building unix plugins
#
BUILD_UNIX_PLUGINS  = 1
#DSO_LDOPTS         += -b elf -G -z defs
DSO_LDOPTS         += -b elf -G
DSO_LDFLAGS        += -nostdlib -L/lib -L/usr/lib -lXm -lXt -lX11 -lgen

# Used for Java compiler
EXPORT_FLAGS += -W l,-Bexport
