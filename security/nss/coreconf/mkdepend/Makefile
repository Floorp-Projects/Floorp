# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

DEPTH		= ../..
CORE_DEPTH	= ../..

MODULE		= coreconf

CSRCS		= \
                cppsetup.c \
                ifparser.c \
                include.c \
                main.c \
                parse.c \
                pr.c

PROGRAM		= mkdepend

# Indicate that this directory builds build tools.
INTERNAL_TOOLS  = 1


include $(DEPTH)/coreconf/config.mk

TARGETS		= $(PROGRAM)
ifeq (,$(filter-out OS2 WIN%,$(OS_TARGET)))
DEFINES		+= -DNO_X11
else
INSTALL		= true
endif

ifdef NATIVE_CC
CC=$(NATIVE_CC)
endif

ifdef NATIVE_FLAGS
OS_CFLAGS=$(NATIVE_FLAGS)
endif

include $(DEPTH)/coreconf/rules.mk

ifdef GNU_CC
OPTIMIZER = -O3
else
ifeq ($(OS_ARCH),SunOS)
OPTIMIZER = -fast
endif
ifeq ($(OS_ARCH),WINNT)
OPTIMIZER = -Ox
endif
endif

DEFINES += -DINCLUDEDIR=\"/usr/include\" -DOBJSUFFIX=\".$(OBJ_SUFFIX)\"

# Redefine MAKE_OBJDIR for just this directory
define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); mkdir $(@D); fi
endef

