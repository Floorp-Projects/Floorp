#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
ifeq ($(OS_RELEASE),B.10.01)
	DEFAULT_IMPL_STRATEGY	 = _CLASSIC
endif

#
# Config stuff for HP-UXB.10.01
#
include $(CORE_DEPTH)/coreconf/HP-UXB.10.mk
