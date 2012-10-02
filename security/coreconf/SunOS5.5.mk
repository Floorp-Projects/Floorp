#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

SOL_CFLAGS += -D_SVID_GETTOD

include $(CORE_DEPTH)/coreconf/SunOS5.mk

ifeq ($(OS_RELEASE),5.5)
	OS_DEFINES += -DSOLARIS2_5
endif
