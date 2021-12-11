#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# On OSF1 V3.2, classic nspr is the default (and only) implementation
# strategy.

#
# Config stuff for DEC OSF/1 V3.2
#
include $(CORE_DEPTH)/coreconf/OSF1.mk

ifeq ($(OS_RELEASE),V3.2)
	OS_CFLAGS += -DOSF1V3
endif
