#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
SHARED_LIBRARY_LIBS = \
	$(DIST)/lib/$(LIB_PREFIX)certhi.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)cryptohi.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certdb.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nsspki.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssdev.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssb.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certsel.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)checker.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)params.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)results.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)top.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)util.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)crlsel.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)store.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pki.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)system.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)module.$(LIB_SUFFIX) \
	$(NULL)

SHARED_LIBRARY_DIRS = \
	../certhigh \
	../cryptohi \
	../pk11wrap \
	../certdb \
	../pki \
	../dev \
	../base \
	../libpkix/pkix/certsel \
	../libpkix/pkix/checker \
	../libpkix/pkix/params \
	../libpkix/pkix/results \
	../libpkix/pkix/top \
	../libpkix/pkix/util \
	../libpkix/pkix/crlsel \
	../libpkix/pkix/store \
	../libpkix/pkix_pl_nss/pki \
	../libpkix/pkix_pl_nss/system \
	../libpkix/pkix_pl_nss/module \
	$(NULL)

ifeq (,$(filter-out WINNT WIN95,$(OS_TARGET)))
ifndef NS_USE_GCC
# Export 'mktemp' to be backward compatible with NSS 3.2.x and 3.3.x
# but do not put it in the import library.  See bug 142575.
DEFINES += -DWIN32_NSS3_DLL_COMPAT
DLLFLAGS += -EXPORT:mktemp=nss_mktemp,PRIVATE
endif
endif
