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
      ARCHFLAG=-m64
  else
      ifeq ($(OS_TEST),i86pc)
        ARCHFLAG=-xarch=amd64
      else
        ARCHFLAG=-xarch=v9
      endif
  endif
else
  ifneq ($(OS_TEST),i86pc)
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
	OS_CFLAGS += $(NOMD_OS_CFLAGS) $(ARCHFLAG)
	ifdef USE_MDUPDATE
		OS_CFLAGS += -MDupdate $(DEPENDENCIES)
	endif
	ifdef BUILD_OPT
	    OPTIMIZER = -O2
	endif
else
	CC         = cc
	CCC        = CC
	ASFLAGS   += -Wa,-P
	OS_CFLAGS += $(NOMD_OS_CFLAGS) $(ARCHFLAG)
	ifndef BUILD_OPT
		OS_CFLAGS  += -xs
	else
		OPTIMIZER = -xO4
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

MKSHLIB  = $(CC) $(DSO_LDOPTS)
ifdef NS_USE_GCC
ifeq (GNU,$(findstring GNU,$(shell `$(CC) -print-prog-name=ld` -v 2>&1)))
	GCC_USE_GNU_LD = 1
endif
endif
ifdef MAPFILE
ifdef NS_USE_GCC
ifdef GCC_USE_GNU_LD
    MKSHLIB += -Wl,--version-script,$(MAPFILE)
else
    MKSHLIB += -Wl,-M,$(MAPFILE)
endif
else
    MKSHLIB += -M $(MAPFILE)
endif
endif
PROCESS_MAP_FILE = grep -v ';-' $(LIBRARY_NAME).def | \
         sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@




# ld options:
# -G: produce a shared object
# -z defs: no unresolved symbols allowed
ifdef NS_USE_GCC
ifeq ($(USE_64), 1)
	DSO_LDOPTS += -m64
endif
	DSO_LDOPTS += -shared -h $(notdir $@)
else
ifeq ($(USE_64), 1)
	ifeq ($(OS_TEST),i86pc)
	    DSO_LDOPTS +=-xarch=amd64
	else
	    DSO_LDOPTS +=-xarch=v9
	endif
endif
	DSO_LDOPTS += -G -h $(notdir $@)
endif

# -KPIC generates position independent code for use in shared libraries.
# (Similarly for -fPIC in case of gcc.)
ifdef NS_USE_GCC
	DSO_CFLAGS += -fPIC
else
	DSO_CFLAGS += -KPIC
endif

NOSUCHFILE   = /solaris-rm-f-sucks

