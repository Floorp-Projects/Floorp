# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
CONFIG_CVS_ID = "@(#) $RCSfile: config.mk,v $ $Revision: 1.16 $ $Date: 2012/11/30 02:40:52 $"

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
ifndef USE_64
DSO_LDOPTS = -bundle
endif
endif

