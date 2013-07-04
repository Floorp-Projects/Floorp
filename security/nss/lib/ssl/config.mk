#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef NISCC_TEST
DEFINES += -DNISCC_TEST
endif

ifdef NSS_NO_PKCS11_BYPASS
DEFINES += -DNO_PKCS11_BYPASS
else
CRYPTOLIB=$(SOFTOKEN_LIB_DIR)/$(LIB_PREFIX)freebl.$(LIB_SUFFIX)

EXTRA_LIBS += \
	$(CRYPTOLIB) \
	$(NULL)
endif

ifeq (,$(filter-out WIN%,$(OS_TARGET)))

# don't want the 32 in the shared library name
SHARED_LIBRARY = $(OBJDIR)/$(DLL_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
IMPORT_LIBRARY = $(OBJDIR)/$(IMPORT_LIB_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION)$(IMPORT_LIB_SUFFIX)

RES = $(OBJDIR)/ssl.res
RESNAME = ssl.rc

ifdef NS_USE_GCC
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-lnss3 \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
else # ! NS_USE_GCC
EXTRA_SHARED_LIBS += \
	$(DIST)/lib/nss3.lib \
	$(DIST)/lib/nssutil3.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)
endif # NS_USE_GCC

else

# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-lnss3 \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)

ifeq ($(OS_ARCH), BeOS)
EXTRA_SHARED_LIBS += -lbe
endif

endif

# Mozilla's mozilla/modules/zlib/src/zconf.h adds the MOZ_Z_ prefix to zlib
# exported symbols, which causes problem when NSS is built as part of Mozilla.
# So we add a NSS_ENABLE_ZLIB variable to allow Mozilla to turn this off.
NSS_ENABLE_ZLIB = 1
ifdef NSS_ENABLE_ZLIB

DEFINES += -DNSS_ENABLE_ZLIB

# If a platform has a system zlib, set USE_SYSTEM_ZLIB to 1 and
# ZLIB_LIBS to the linker command-line arguments for the system zlib
# (for example, -lz) in the platform's config file in coreconf.
ifdef USE_SYSTEM_ZLIB
OS_LIBS += $(ZLIB_LIBS)
else
ZLIB_LIBS = $(DIST)/lib/$(LIB_PREFIX)zlib.$(LIB_SUFFIX)
EXTRA_LIBS += $(ZLIB_LIBS)
endif

endif
