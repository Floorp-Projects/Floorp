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
# Config stuff for Solaris 2.4 on x86
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC		= gcc
	OS_CFLAGS	+= -Wall -Wno-format
	CCC		= g++
	CCC		+= -Wall -Wno-format
	ASFLAGS		+= -x assembler-with-cpp
	OS_CFLAGS	+= $(NOMD_OS_CFLAGS)
	ifdef USE_MDUPDATE
		OS_CFLAGS += -MDupdate $(DEPENDENCIES)
	endif
else
	CC		= cc
	CCC		= CC
	ASFLAGS		+= -Wa,-P
	OS_CFLAGS	+= $(NOMD_OS_CFLAGS)
endif

CPU_ARCH	= x86

MKSHLIB		= $(LD)
MKSHLIB		+= $(DSO_LDOPTS)
NOSUCHFILE	= /solx86-rm-f-sucks
RANLIB		= echo

# for purify
NOMD_OS_CFLAGS	+= -DSVR4 -DSYSV -D_REENTRANT -DSOLARIS -D__svr4__ -Di386

DSO_LDOPTS	+= -G
