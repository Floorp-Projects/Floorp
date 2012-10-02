#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Config stuff for OS_TARGET=WINNT
#

include $(CORE_DEPTH)/coreconf/WIN32.mk

DEFINES += -DWINNT

#
# Win NT needs -GT so that fibers can work
#
OS_CFLAGS += -GT

# WINNT uses the lib prefix, Win95 and WinCE don't
NSPR31_LIB_PREFIX = lib
