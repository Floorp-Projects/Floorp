#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# On HP-UX 10.30 and 11.00, the default implementation strategy is
# pthreads.  Classic nspr and pthreads-user are also available.

ifeq ($(OS_RELEASE),B.11.00)
OS_CFLAGS		+= -DHPUX10
DEFAULT_IMPL_STRATEGY = _PTH
endif

#
# Config stuff for HP-UXB.11.00.
#
include $(CORE_DEPTH)/coreconf/HP-UXB.11.mk
