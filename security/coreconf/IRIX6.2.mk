#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# catch unresolved symbols

SHLIB_LD_OPTS += -no_unresolved

include $(CORE_DEPTH)/coreconf/IRIX6.mk

OS_CFLAGS += -DIRIX6_2
