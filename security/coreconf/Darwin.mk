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

DEFAULT_COMPILER = cc

CC		= cc
CCC		= c++
RANLIB		= ranlib

ifeq (86,$(findstring 86,$(OS_TEST)))
OS_REL_CFLAGS	= -Di386
CPU_ARCH	= i386
else
OS_REL_CFLAGS	= -Dppc
CPU_ARCH	= ppc
endif

ifneq (,$(NEXT_ROOT))
    GCC_VERSION_FULL := $(shell $(CC) -v 2>&1 | grep "gcc version" | sed -e "s/^.*gcc version[  ]*//" | awk '{ print $$1 }')
    GCC_VERSION_MAJOR := $(shell echo $(GCC_VERSION_FULL) | awk -F. '{ print $$1 }')
    GCC_VERSION_MINOR := $(shell echo $(GCC_VERSION_FULL) | awk -F. '{ print $$2 }')
    GCC_VERSION := $(GCC_VERSION_MAJOR).$(GCC_VERSION_MINOR)

    DARWIN_SDK_CFLAGS := -nostdinc

    ifeq (,$(filter-out 2 3,$(GCC_VERSION_MAJOR)))
        # GCC <= 3
        DARWIN_TARGET_ARCH_LIB := darwin
        DARWIN_SDK_CFLAGS += -isystem $(NEXT_ROOT)/usr/include/gcc/darwin/$(GCC_VERSION)
    else
        # GCC >= 4
        CPU_ARCH_LONG := $(shell uname -p)
        DARWIN_TARGET_ARCH_LIB := $(CPU_ARCH_LONG)-apple-darwin$(shell echo $NEXT_ROOT | perl -pe 's/MacOSX10\.([\d]*)//;if ($$1) {$$_=$$1+4;} else {$$_="'${OS_RELEASE}'";s/(\d+)//;$$_=$$1;}')
        DARWIN_SDK_CFLAGS += -isystem $(NEXT_ROOT)/usr/lib/gcc/$(DARWIN_TARGET_ARCH_LIB)/$(GCC_VERSION_FULL)/include
    endif

    DARWIN_SDK_CFLAGS += -isystem $(NEXT_ROOT)/usr/include -F$(NEXT_ROOT)/System/Library/Frameworks
    DARWIN_SDK_LDFLAGS := -L$(NEXT_ROOT)/usr/lib/gcc/$(DARWIN_TARGET_ARCH_LIB) -L$(NEXT_ROOT)/usr/lib/gcc/$(DARWIN_TARGET_ARCH_LIB)/$(GCC_VERSION_FULL) -L$(NEXT_ROOT)/usr/lib

    ifneq (,$(shell find $(NEXT_ROOT)/Library/Frameworks -maxdepth 0))
        DARWIN_SDK_CFLAGS += -F$(NEXT_ROOT)/Library/Frameworks
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

OS_CFLAGS	= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wmost -fpascal-strings -no-cpp-precomp -fno-common -pipe -DDARWIN -DHAVE_STRERROR -DHAVE_BSD_FLOCK $(DARWIN_SDK_CFLAGS)

ifdef BUILD_OPT
OPTIMIZER	= -O2
endif

ARCH		= darwin

DSO_CFLAGS	= -fPIC
# May override this with -bundle to create a loadable module.
DSO_LDOPTS	= -dynamiclib -compatibility_version 1 -current_version 1 -install_name @executable_path/$(notdir $@) -headerpad_max_install_names $(DARWIN_SDK_LDFLAGS)

MKSHLIB		= $(CC) -arch $(CPU_ARCH) $(DSO_LDOPTS)
DLL_SUFFIX	= dylib
PROCESS_MAP_FILE = grep -v ';+' $< | grep -v ';-' | \
                sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,^,_,' > $@

G++INCLUDES	= -I/usr/include/g++
