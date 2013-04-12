#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Config stuff for HP-UX
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

ifeq ($(OS_TEST),ia64)
	CPU_ARCH = ia64
	CPU_TAG = _$(CPU_ARCH)
	ifneq ($(USE_64),1)
		64BIT_TAG = _32
	endif
	DLL_SUFFIX = so
else
	CPU_ARCH = hppa
	DLL_SUFFIX = sl
endif
CC         = cc
CCC        = CC
ifndef NS_USE_GCC
OS_CFLAGS  += -Ae
endif
OS_CFLAGS  += $(DSO_CFLAGS) -DHPUX -D$(CPU_ARCH) -D_HPUX_SOURCE -D_USE_BIG_FDS

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

ifdef NS_USE_GCC
LD = $(CC)
endif
MKSHLIB			= $(LD) $(DSO_LDOPTS) $(RPATH)
ifdef MAPFILE
ifndef NS_USE_GCC
MKSHLIB += -c $(MAPFILE)
else
MKSHLIB += -Wl,-c,$(MAPFILE)
endif
endif
PROCESS_MAP_FILE = grep -v ';+' $< | grep -v ';-' | \
         sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,^,+e ,' > $@

ifndef NS_USE_GCC
DSO_LDOPTS		= -b +h $(notdir $@)
RPATH			= +b '$$ORIGIN'
else
DSO_LDOPTS		= -shared -Wl,+h,$(notdir $@)
RPATH			= -Wl,+b,'$$ORIGIN'
endif
ifneq ($(OS_TEST),ia64)
# pa-risc
ifndef USE_64
RPATH			=
endif
endif

# +Z generates position independent code for use in shared libraries.
ifndef NS_USE_GCC
DSO_CFLAGS = +Z
else
DSO_CFLAGS = -fPIC
ASFLAGS   += -x assembler-with-cpp
endif
