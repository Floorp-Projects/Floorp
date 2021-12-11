#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# On HP-UX 10.10 and 10.20, the default implementation strategy is
# pthreads (actually DCE threads).  Classic nspr is also available.

ifeq ($(OS_RELEASE),B.10.20)
	DEFAULT_IMPL_STRATEGY = _PTH
endif

#
# Config stuff for HP-UXB.10.20
#
include $(CORE_DEPTH)/coreconf/HP-UXB.10.mk

OS_CFLAGS		+= -DHPUX10_20

ifeq ($(USE_PTHREADS),1)
	OS_CFLAGS	+= -D_REENTRANT
endif
