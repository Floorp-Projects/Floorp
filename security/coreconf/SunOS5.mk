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
# Config stuff for SunOS5.x
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

#
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
	LOCAL_THREADS_ONLY = 1
	ifndef NS_USE_NATIVE
		NS_USE_GCC = 1
	endif
endif

# Sun's WorkShop defines v8, v8plus and v9 architectures.
# gcc on Solaris defines v8 and v9 "cpus".  
# gcc's v9 is equivalent to Workshop's v8plus.
# gcc apparently has no equivalent to Workshop's v9
# We always use Sun's assembler and linker, which use Sun's naming convention.

ifeq ($(USE_64), 1)
  ifdef NS_USE_GCC
      ARCHFLAG= UNKNOWN 
  else
      ARCHFLAG=-xarch=v9
  endif
      LD=/usr/ccs/bin/ld
else
  ifdef NS_USE_GCC
    ifdef USE_HYBRID
      ARCHFLAG=-mcpu=v9 -Wa,-xarch=v8plus
    else
      ARCHFLAG=-mcpu=v8
    endif
  else
    ifdef USE_HYBRID
      ARCHFLAG=-xarch=v8plus
    else
      ARCHFLAG=-xarch=v8
    endif
  endif
endif

#
# The default implementation strategy for Solaris is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
else
	ifeq ($(LOCAL_THREADS_ONLY),1)
		IMPL_STRATEGY = _LOCAL
	endif
endif

#
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
	IMPL_STRATEGY =
endif

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC         = gcc
	OS_CFLAGS += -Wall -Wno-format
	CCC        = g++
	CCC       += -Wall -Wno-format
	ASFLAGS	  += -x assembler-with-cpp
	OS_CFLAGS += $(NOMD_OS_CFLAGS)
	ifdef USE_MDUPDATE
		OS_CFLAGS += -MDupdate $(DEPENDENCIES)
	endif
	OS_CFLAGS += $(ARCHFLAG)
else
	CC         = cc
	CCC        = CC
	ASFLAGS   += -Wa,-P
	OS_CFLAGS += $(NOMD_OS_CFLAGS) $(ARCHFLAG)
	ifndef BUILD_OPT
		OS_CFLAGS  += -xs
#	else
#		OPTIMIZER += -fast
	endif

endif

INCLUDES   += -I/usr/dt/include -I/usr/openwin/include

RANLIB      = echo
CPU_ARCH    = sparc
OS_DEFINES += -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS

ifneq ($(LOCAL_THREADS_ONLY),1)
	OS_DEFINES		+= -D_REENTRANT
endif

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS += $(DSO_CFLAGS) $(OS_DEFINES) $(SOL_CFLAGS)

MKSHLIB  = $(LD) $(DSO_LDOPTS)

# ld options:
# -G: produce a shared object
# -z defs: no unresolved symbols allowed
DSO_LDOPTS += -G -h $(notdir $@)

# -KPIC generates position independent code for use in shared libraries.
# (Similarly for -fPIC in case of gcc.)
ifdef NS_USE_GCC
	DSO_CFLAGS += -fPIC
else
	DSO_CFLAGS += -KPIC
endif

NOSUCHFILE   = /solaris-rm-f-sucks

