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
# Config stuff for ReliantUNIX
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	## gcc-2.7.2 homebrewn
	CC          = gcc
	CCC         = g++
	AS          = $(CC)
	ASFLAGS     += -x assembler-with-cpp
	LD          = gld
	ODD_CFLAGS  = -pipe -Wall -Wno-format
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
PROCESS_MAP_FILE = cp $(LIBRARY_NAME).def $@

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
