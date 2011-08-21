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

DEFAULT_COMPILER = gcc

CC		= gcc
CCC		= g++
RANLIB		= ranlib

ifndef CPU_ARCH
# When cross-compiling, CPU_ARCH should already be defined as the target
# architecture, set to powerpc or i386.
CPU_ARCH	:= $(shell uname -p)
endif

ifeq (,$(filter-out i%86,$(CPU_ARCH)))
ifdef USE_64
CC              += -arch x86_64
override CPU_ARCH	= x86_64
else
OS_REL_CFLAGS	= -Di386
CC              += -arch i386
override CPU_ARCH	= x86
endif
else
OS_REL_CFLAGS	= -Dppc
CC              += -arch ppc
endif

ifneq (,$(MACOS_SDK_DIR))
    GCC_VERSION_FULL := $(shell $(CC) -dumpversion)
    GCC_VERSION_MAJOR := $(shell echo $(GCC_VERSION_FULL) | awk -F. '{ print $$1 }')
    GCC_VERSION_MINOR := $(shell echo $(GCC_VERSION_FULL) | awk -F. '{ print $$2 }')
    GCC_VERSION = $(GCC_VERSION_MAJOR).$(GCC_VERSION_MINOR)

    ifeq (,$(filter-out 2 3,$(GCC_VERSION_MAJOR)))
        # GCC <= 3
        DARWIN_SDK_FRAMEWORKS = -F$(MACOS_SDK_DIR)/System/Library/Frameworks
        ifneq (,$(shell find $(MACOS_SDK_DIR)/Library/Frameworks -maxdepth 0))
            DARWIN_SDK_FRAMEWORKS += -F$(MACOS_SDK_DIR)/Library/Frameworks
        endif
        DARWIN_SDK_CFLAGS = -nostdinc -isystem $(MACOS_SDK_DIR)/usr/include/gcc/darwin/$(GCC_VERSION) -isystem $(MACOS_SDK_DIR)/usr/include $(DARWIN_SDK_FRAMEWORKS)
        DARWIN_SDK_LDFLAGS = -L$(MACOS_SDK_DIR)/usr/lib/gcc/darwin -L$(MACOS_SDK_DIR)/usr/lib/gcc/darwin/$(GCC_VERSION_FULL) -L$(MACOS_SDK_DIR)/usr/lib
        DARWIN_SDK_SHLIBFLAGS = $(DARWIN_SDK_LDFLAGS) $(DARWIN_SDK_FRAMEWORKS)
        NEXT_ROOT = $(MACOS_SDK_DIR)
        export NEXT_ROOT
    else
        # GCC >= 4
        DARWIN_SDK_CFLAGS = -isysroot $(MACOS_SDK_DIR)
        ifneq (4.0.0,$(GCC_VERSION_FULL))
            # gcc > 4.0.0 passes -syslibroot to ld based on -isysroot.
            # Don't add -isysroot to DARWIN_SDK_LDFLAGS, because the programs
            # that are linked with those flags also get DARWIN_SDK_CFLAGS.
            DARWIN_SDK_SHLIBFLAGS = -isysroot $(MACOS_SDK_DIR)
        else
            # gcc 4.0.0 doesn't pass -syslibroot to ld, it needs to be
            # explicit.
            DARWIN_SDK_LDFLAGS = -Wl,-syslibroot,$(MACOS_SDK_DIR)
            DARWIN_SDK_SHLIBFLAGS = $(DARWIN_SDK_LDFLAGS)
        endif
    endif

    LDFLAGS += $(DARWIN_SDK_LDFLAGS)
endif

# "Commons" are tentative definitions in a global scope, like this:
#     int x;
# The meaning of a common is ambiguous.  It may be a true definition:
#     int x = 0;
# or it may be a declaration of a symbol defined in another file:
#     extern int x;
# Use the -fno-common option to force all commons to become true
# definitions so that the linker can catch multiply-defined symbols.
# Also, common symbols are not allowed with Darwin dynamic libraries.

OS_CFLAGS	= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -fno-common -pipe -DDARWIN -DHAVE_STRERROR -DHAVE_BSD_FLOCK $(DARWIN_SDK_CFLAGS)

ifdef BUILD_OPT
ifeq (11,$(ALLOW_OPT_CODE_SIZE)$(OPT_CODE_SIZE))
	OPTIMIZER       = -Oz
else
	OPTIMIZER	= -O2
endif
ifdef MOZ_DEBUG_SYMBOLS
	ifdef MOZ_DEBUG_FLAGS
		OPTIMIZER += $(MOZ_DEBUG_FLAGS)
	else
		OPTIMIZER += -gdwarf-2 -gfull
	endif
endif
endif

ARCH		= darwin

DSO_CFLAGS	= -fPIC
# May override this with different compatibility and current version numbers.
DARWIN_DYLIB_VERSIONS = -compatibility_version 1 -current_version 1
# May override this with -bundle to create a loadable module.
DSO_LDOPTS	= -dynamiclib $(DARWIN_DYLIB_VERSIONS) -install_name @executable_path/$(notdir $@) -headerpad_max_install_names

MKSHLIB		= $(CC) $(DSO_LDOPTS) $(DARWIN_SDK_SHLIBFLAGS)
DLL_SUFFIX	= dylib
PROCESS_MAP_FILE = grep -v ';+' $< | grep -v ';-' | \
                sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,^,_,' > $@

USE_SYSTEM_ZLIB = 1
ZLIB_LIBS	= -lz
