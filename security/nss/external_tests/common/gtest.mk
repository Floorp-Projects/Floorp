#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include ../../cmd/platlibs.mk
include ../../cmd/platrules.mk

MKPROG = $(CCC)
MKSHLIB	= $(CCC) $(DSO_LDOPTS) $(DARWIN_SDK_SHLIBFLAGS)

ifeq (WINNT,$(OS_ARCH))
    # -EHsc because gtest has exception handlers
    OS_CFLAGS += -EHsc -nologo
    # http://www.suodenjoki.dk/us/archive/2010/min-max.htm
    OS_CFLAGS += -DNOMINMAX

    # Linking to winsock to get htonl
    OS_LIBS += Ws2_32.lib

    # On windows, we need to create the parent directory
    # Needed because we include files from a subdirectory
    MAKE_OBJDIR = $(INSTALL) -D $(dir $@)
else
    CXXFLAGS += -std=c++0x
endif
