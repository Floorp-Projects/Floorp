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
# Config stuff for HP-UX
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

CPU_ARCH   = hppa
DLL_SUFFIX = sl
CC         = cc
CCC        = CC
OS_CFLAGS  += -Ae $(DSO_CFLAGS) -DHPUX -D$(CPU_ARCH) -D_HPUX_SOURCE

ifeq ($(DEFAULT_IMPL_STRATEGY),_PTH)
	USE_PTHREADS = 1
	ifeq ($(CLASSIC_NSPR),1)
		USE_PTHREADS =
		IMPL_STRATEGY = _CLASSIC
	endif
	ifeq ($(PTHREADS_USER),1)
		USE_PTHREADS =
		IMPL_STRATEGY = _PTH_USER
	endif
endif

ifdef PTHREADS_USER
	OS_CFLAGS	+= -D_POSIX_C_SOURCE=199506L
endif

LDFLAGS			= -z -Wl,+s

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -b +h $(notdir $@)
DSO_LDFLAGS		=

# +Z generates position independent code for use in shared libraries.
DSO_CFLAGS = +Z
