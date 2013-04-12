#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# On OSF1 V5.0, pthreads is the default implementation strategy.
# Classic nspr is also available.

ifneq ($(OS_RELEASE),V3.2)
	USE_PTHREADS = 1
	ifeq ($(CLASSIC_NSPR), 1)
		USE_PTHREADS =
		IMPL_STRATEGY := _CLASSIC
	endif
endif

#
# Config stuff for DEC OSF/1 V5.0
#
include $(CORE_DEPTH)/coreconf/OSF1.mk
