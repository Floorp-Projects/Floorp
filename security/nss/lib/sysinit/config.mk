#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
#  Override TARGETS variable so that only static libraries
#  are specifed as dependencies within rules.mk.
#

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))

ifdef NS_USE_GCC
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4\
	$(NULL)
else # ! NS_USE_GCC
EXTRA_SHARED_LIBS += \
	$(DIST)/lib/nssutil3.lib \
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
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)

endif


# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)

ifeq ($(OS_TARGET),SunOS)
ifeq ($(BUILD_SUN_PKG), 1)
# The -R '$ORIGIN' linker option instructs this library to search for its
# dependencies in the same directory where it resides.
ifeq ($(USE_64), 1)
MKSHLIB += -R '$$ORIGIN:/usr/lib/mps/secv1/64:/usr/lib/mps/64'
else
MKSHLIB += -R '$$ORIGIN:/usr/lib/mps/secv1:/usr/lib/mps'
endif
else
MKSHLIB += -R '$$ORIGIN'
endif
endif

ifeq ($(OS_ARCH), HP-UX) 
ifneq ($(OS_TEST), ia64)
# pa-risc
ifeq ($(USE_64), 1)
MKSHLIB += +b '$$ORIGIN'
endif
endif
endif
