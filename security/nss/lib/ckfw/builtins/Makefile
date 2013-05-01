# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
MAKEFILE_CVS_ID = "@(#) $RCSfile$ $Revision$ $Date$"

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk

EXTRA_LIBS = \
	$(DIST)/lib/$(LIB_PREFIX)nssckfw.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssb.$(LIB_SUFFIX) \
	$(NULL)

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))

ifdef NS_USE_GCC
EXTRA_SHARED_LIBS += \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
else 
EXTRA_SHARED_LIBS += \
        $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.lib \
        $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.lib \
        $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
        $(NULL)
endif # NS_USE_GCC
else

EXTRA_SHARED_LIBS += \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif


include $(CORE_DEPTH)/coreconf/rules.mk

# Generate certdata.c.

# By default, use the unmodified certdata.txt.
ifndef NSS_CERTDATA_TXT
NSS_CERTDATA_TXT = certdata.txt
endif

$(OBJDIR)/certdata.c: $(NSS_CERTDATA_TXT) certdata.perl
	$(PERL) certdata.perl < $(NSS_CERTDATA_TXT) > $@
