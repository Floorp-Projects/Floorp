#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

DEFAULT_COMPILER = cc

INCLUDES += -I/usr/dt/include -I/usr/openwin/include -I/home/motif/usr/include

# SunOS 4 _requires_ that shared libs have a version number.
# XXX FIXME: Version number should use NSPR_VERSION_NUMBER?
DLL_SUFFIX = so.1.0
CC         = gcc
RANLIB     = ranlib
CPU_ARCH   = sparc

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS += -Wall -Wno-format -Wno-switch -DSUNOS4
OS_CFLAGS      += $(DSO_CFLAGS) $(NOMD_OS_CFLAGS)
MKSHLIB         = $(LD)
MKSHLIB        += $(DSO_LDOPTS)
NOSUCHFILE      = /solaris-rm-f-sucks
DSO_LDOPTS      =

# -fPIC generates position-independent code for use in a shared library.
DSO_CFLAGS += -fPIC
