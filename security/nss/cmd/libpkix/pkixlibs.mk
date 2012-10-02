# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq (,$(filter-out WIN%,$(OS_TARGET)))

ifdef NS_USE_GCC
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-lpkixutil \
	$(NULL)
else # ! NS_USE_GCC
EXTRA_SHARED_LIBS += \
	$(DIST)/lib/pkixutil.lib \
	$(NULL)
endif # NS_USE_GCC

else

EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib/ \
	-lpkixutil \
	$(NULL)

endif

