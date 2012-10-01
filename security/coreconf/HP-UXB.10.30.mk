#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# On HP-UX 10.30 and 11.00, the default implementation strategy is
# pthreads.  Classic nspr and pthreads-user are also available.

ifeq ($(OS_RELEASE),B.10.30)
	DEFAULT_IMPL_STRATEGY	 = _PTH
endif

#
# Config stuff for HP-UXB.10.30.
#
include $(CORE_DEPTH)/coreconf/HP-UXB.10.mk

OS_CFLAGS		+= -DHPUX10_30

#
# To use the true pthread (kernel thread) library on 10.30 and
# 11.00, we should define _POSIX_C_SOURCE to be 199506L.
# The _REENTRANT macro is deprecated.
#

ifdef USE_PTHREADS
	OS_CFLAGS	+= -D_POSIX_C_SOURCE=199506L
endif
