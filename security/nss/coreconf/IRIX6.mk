#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/IRIX.mk

ifndef NS_USE_GCC
	ifneq ($(USE_N32),1)
	OS_CFLAGS  += -32
	endif
	ODD_CFLAGS += -multigot
endif

ifeq ($(USE_PTHREADS),1)
OS_LIBS += -lpthread
endif
