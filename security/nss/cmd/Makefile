#! gmake
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

CORE_DEPTH = ..
DEPTH = ..

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk

ifdef BUILD_LIBPKIX_TESTS
DIRS += libpkix
endif

LOWHASHTEST_SRCDIR=
ifeq ($(FREEBL_LOWHASH),1)
LOWHASHTEST_SRCDIR = lowhashtest  # Add the lowhashtest directory to DIRS.
endif

INCLUDES += \
	-I$(DIST)/../public/security \
	-I./include \
	$(NULL)

include $(CORE_DEPTH)/coreconf/rules.mk

symbols::
	@echo "TARGETS	= $(TARGETS)"
