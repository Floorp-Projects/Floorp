#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

