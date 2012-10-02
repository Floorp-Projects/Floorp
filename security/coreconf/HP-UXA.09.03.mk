#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# On HP-UX 9, the default (and only) implementation strategy is
# classic nspr.
#
ifeq ($(OS_RELEASE),A.09.03)
	DEFAULT_IMPL_STRATEGY	 = _CLASSIC
endif

#
# Config stuff for HP-UXA.09.03
#
include $(CORE_DEPTH)/coreconf/HP-UXA.09.mk
