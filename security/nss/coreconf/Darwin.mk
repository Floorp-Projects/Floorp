#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk
include $(CORE_DEPTH)/coreconf/Werror.mk

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
CCC             += -arch x86_64
override CPU_ARCH	= x86_64
else
OS_REL_CFLAGS	= -Di386
CC              += -arch i386
CCC             += -arch i386
override CPU_ARCH	= x86
endif
else
ifeq (arm,$(CPU_ARCH))
# Nothing set for arm currently.
else
OS_REL_CFLAGS	= -Dppc
CC              += -arch ppc
CCC             += -arch ppc
endif
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

OS_CFLAGS	= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -fno-common -pipe -DDARWIN -DHAVE_STRERROR -DHAVE_BSD_FLOCK $(DARWIN_SDK_CFLAGS)

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
ifdef MAPFILE
	MKSHLIB += -exported_symbols_list $(MAPFILE)
endif
PROCESS_MAP_FILE = grep -v ';+' $< | grep -v ';-' | \
                sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,^,_,' > $@

USE_SYSTEM_ZLIB = 1
ZLIB_LIBS	= -lz

# The system sqlite library in the latest version of Mac OS X often becomes
# newer than the sqlite library in NSS. This may result in certain Mac OS X
# system libraries having unresolved sqlite symbols during the shlibsign step
# of the NSS build when we set DYLD_LIBRARY_PATH to the NSS lib directory and
# the NSS libsqlite3.dylib is used instead of the system one. So just use the
# system sqlite library on Mac, if it's sufficiently new.

SYS_SQLITE3_VERSION_FULL := $(shell /usr/bin/sqlite3 -version | awk '{print $$1}')
SYS_SQLITE3_VERSION_MAJOR := $(shell echo $(SYS_SQLITE3_VERSION_FULL) | awk -F. '{ print $$1 }')
SYS_SQLITE3_VERSION_MINOR := $(shell echo $(SYS_SQLITE3_VERSION_FULL) | awk -F. '{ print $$2 }')

ifeq (3,$(SYS_SQLITE3_VERSION_MAJOR))
    ifeq (,$(filter-out 0 1 2 3 4,$(SYS_SQLITE3_VERSION_MINOR)))
        # sqlite <= 3.4.x is too old, it doesn't provide sqlite3_file_control
    else
        NSS_USE_SYSTEM_SQLITE = 1
    endif
endif

include $(CORE_DEPTH)/coreconf/sanitizers.mk
DARWIN_SDK_SHLIBFLAGS += $(SANITIZER_LDFLAGS)
