#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
CRYPTOLIB=$(DIST)/lib/$(LIB_PREFIX)freebl.$(LIB_SUFFIX)

EXTRA_LIBS += \
	$(CRYPTOLIB) \
	$(NULL)

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))

# don't want the 32 in the shared library name
SHARED_LIBRARY = $(OBJDIR)/$(DLL_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
IMPORT_LIBRARY = $(OBJDIR)/$(IMPORT_LIB_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION)$(IMPORT_LIB_SUFFIX)

RES = $(OBJDIR)/$(LIBRARY_NAME).res
RESNAME = $(LIBRARY_NAME).rc

ifdef NS_USE_GCC
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-l$(SQLITE_LIB_NAME) \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
else # ! NS_USE_GCC

EXTRA_SHARED_LIBS += \
	$(DIST)/lib/$(SQLITE_LIB_NAME).lib \
	$(NSSUTIL_LIB_DIR)/nssutil3.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)
endif # NS_USE_GCC

else

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-l$(SQLITE_LIB_NAME) \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif

ifeq ($(OS_TARGET),AIX)
OS_LIBS += -lpthread
endif

ifeq ($(OS_TARGET),SunOS)
OS_LIBS += -lbsm 
endif

ifeq ($(OS_TARGET),WINCE)
DEFINES += -DDBM_USING_NSPR
endif
