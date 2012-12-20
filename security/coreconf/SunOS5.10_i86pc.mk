#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/SunOS5.mk

ifeq ($(USE_64),1)
    CPU_ARCH		= x86_64
else
    CPU_ARCH		= x86
    OS_DEFINES		+= -Di386
endif

ifeq ($(OS_RELEASE),5.10_i86pc)
	OS_DEFINES += -DSOLARIS2_10
endif

OS_LIBS += -lthread -lnsl -lsocket -lposix4 -ldl -lc
