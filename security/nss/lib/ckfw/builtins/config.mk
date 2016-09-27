# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
#  Override TARGETS variable so that only shared libraries
#  are specifed as dependencies within rules.mk.
#

TARGETS        = $(SHARED_LIBRARY)
LIBRARY        =
IMPORT_LIBRARY =
PROGRAM        =

ifeq (,$(filter-out WIN%,$(OS_TARGET)))
    SHARED_LIBRARY = $(OBJDIR)/$(DLL_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
    RES = $(OBJDIR)/$(LIBRARY_NAME).res
    RESNAME = $(LIBRARY_NAME).rc
endif

ifdef BUILD_IDG
    DEFINES += -DNSSDEBUG
endif

# Needed for compilation of $(OBJDIR)/certdata.c
INCLUDES += -I.

#
# To create a loadable module on Darwin, we must use -bundle.
#
ifeq ($(OS_TARGET),Darwin)
DSO_LDOPTS = -bundle
endif

ifdef USE_GCOV
DSO_LDOPTS += --coverage
endif
