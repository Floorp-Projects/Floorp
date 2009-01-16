#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# set RPATH-type linker instructions here so they can be used in the shared
# version and in the mixed (static nss libs/shared NSPR libs) version.

ifeq ($(OS_ARCH), SunOS) 
ifeq ($(BUILD_SUN_PKG), 1)
ifeq ($(USE_64), 1)
EXTRA_SHARED_LIBS += -R '$$ORIGIN/../lib:/usr/lib/mps/secv1/64:/usr/lib/mps/64'
else
EXTRA_SHARED_LIBS += -R '$$ORIGIN/../lib:/usr/lib/mps/secv1:/usr/lib/mps'
endif
else
EXTRA_SHARED_LIBS += -R '$$ORIGIN/../lib'
endif
endif

ifeq ($(OS_ARCH), Linux)
ifeq ($(USE_64), 1)
EXTRA_SHARED_LIBS += -Wl,-rpath,'$$ORIGIN/../lib64:$$ORIGIN/../lib'
else
EXTRA_SHARED_LIBS += -Wl,-rpath,'$$ORIGIN/../lib'
endif
endif

ifeq ($(OS_ARCH), HP-UX) 
ifeq ($(OS_TEST), ia64)
EXTRA_SHARED_LIBS += -Wl,+b,'$$ORIGIN/../lib'
else
# pa-risc
ifeq ($(USE_64), 1)
EXTRA_SHARED_LIBS += \
-Wl,+b,'$$ORIGIN/../../lib/pa20_64:$$ORIGIN/../../lib/64:$$ORIGIN/../lib'
else
EXTRA_SHARED_LIBS += -Wl,+b,'$$ORIGIN/../lib'
endif
endif
endif

SQLITE=-lsqlite3

ifdef USE_STATIC_LIBS

# can't do this in manifest.mn because OS_ARCH isn't defined there.
ifeq ($(OS_ARCH), WINNT)

DEFINES += -DNSS_USE_STATIC_LIBS
# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
CRYPTOLIB=$(DIST)/lib/$(LIB_PREFIX)freebl.$(LIB_SUFFIX)
ifdef MOZILLA_SECURITY_BUILD
	CRYPTOLIB=$(DIST)/lib/crypto.lib
endif
ifdef MOZILLA_BSAFE_BUILD
	CRYPTOLIB+=$(DIST)/lib/bsafe$(BSAFEVER).lib
	CRYPTOLIB+=$(DIST)/lib/freebl.lib
endif

PKIXLIB = \
	$(DIST)/lib/$(LIB_PREFIX)pkixcertsel.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixchecker.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixparams.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixresults.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixtop.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixutil.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixcrlsel.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixstore.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixpki.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixsystem.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixmodule.$(LIB_SUFFIX)

EXTRA_LIBS += \
	$(DIST)/lib/$(LIB_PREFIX)smime.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)ssl.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nss.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)ssl.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)sectool.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkcs12.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkcs7.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certhi.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)cryptohi.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certdb.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)softokn.$(LIB_SUFFIX) \
	$(CRYPTOLIB) \
	$(DIST)/lib/$(LIB_PREFIX)nssutil.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nsspki.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssdev.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssb.$(LIB_SUFFIX) \
	$(PKIXLIB) \
	$(DIST)/lib/$(LIB_PREFIX)dbm.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)sqlite3.$(LIB_SUFFIX) \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.$(LIB_SUFFIX) \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.$(LIB_SUFFIX) \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.$(LIB_SUFFIX) \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(OS_LIBS)
#OS_LIBS += \
	wsock32.lib \
	winmm.lib \
	$(NULL)
else

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
CRYPTOLIB=$(DIST)/lib/$(LIB_PREFIX)freebl.$(LIB_SUFFIX)
ifdef MOZILLA_SECURITY_BUILD
	CRYPTOLIB=$(DIST)/lib/$(LIB_PREFIX)crypto.$(LIB_SUFFIX)
endif
ifdef MOZILLA_BSAFE_BUILD
	CRYPTOLIB+=$(DIST)/lib/$(LIB_PREFIX)bsafe.$(LIB_SUFFIX)
	CRYPTOLIB+=$(DIST)/lib/$(LIB_PREFIX)freebl.$(LIB_SUFFIX)
endif

PKIXLIB = \
	$(DIST)/lib/$(LIB_PREFIX)pkixtop.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixutil.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixsystem.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixcrlsel.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixmodule.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixstore.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixparams.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixchecker.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixpki.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixtop.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixresults.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkixcertsel.$(LIB_SUFFIX)

EXTRA_LIBS += \
	$(DIST)/lib/$(LIB_PREFIX)smime.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)ssl.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nss.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)ssl.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)sectool.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkcs12.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pkcs7.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certhi.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)cryptohi.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certhi.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nsspki.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)softokn.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certdb.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nsspki.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssdev.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssb.$(LIB_SUFFIX) \
	$(CRYPTOLIB) \
	$(DIST)/lib/$(LIB_PREFIX)nssutil.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)dbm.$(LIB_SUFFIX) \
	$(PKIXLIB) \
	$(DIST)/lib/$(LIB_PREFIX)nss.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)pk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)certhi.$(LIB_SUFFIX) \
	$(NULL)

ifeq ($(OS_ARCH), AIX) 
EXTRA_SHARED_LIBS += -brtl 
endif

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	$(SQLITE) \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif

ifeq ($(OS_TARGET), SunOS)
OS_LIBS += -lbsm
endif

else # USE_STATIC_LIBS
# can't do this in manifest.mn because OS_ARCH isn't defined there.
ifeq ($(OS_ARCH), WINNT)

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
EXTRA_LIBS += \
	$(DIST)/lib/$(LIB_PREFIX)sectool.$(LIB_SUFFIX) \
	$(DIST)/lib/$(IMPORT_LIB_PREFIX)nssutil3$(IMPORT_LIB_SUFFIX) \
	$(DIST)/lib/$(IMPORT_LIB_PREFIX)smime3$(IMPORT_LIB_SUFFIX) \
	$(DIST)/lib/$(IMPORT_LIB_PREFIX)ssl3$(IMPORT_LIB_SUFFIX) \
	$(DIST)/lib/$(IMPORT_LIB_PREFIX)nss3$(IMPORT_LIB_SUFFIX) \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4$(IMPORT_LIB_SUFFIX) \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4$(IMPORT_LIB_SUFFIX) \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4$(IMPORT_LIB_SUFFIX) \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(OS_LIBS)
#OS_LIBS += \
	wsock32.lib \
	winmm.lib \
	$(NULL)
else

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
EXTRA_LIBS += \
	$(DIST)/lib/$(LIB_PREFIX)sectool.$(LIB_SUFFIX) \
	$(NULL)

ifeq ($(OS_ARCH), AIX) 
EXTRA_SHARED_LIBS += -brtl 
endif

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-lssl3 \
	-lsmime3 \
	-lnss3 \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif

endif # USE_STATIC_LIBS

# If a platform has a system zlib, set USE_SYSTEM_ZLIB to 1 and
# ZLIB_LIBS to the linker command-line arguments for the system zlib
# (for example, -lz) in the platform's config file in coreconf.
ifndef USE_SYSTEM_ZLIB
ZLIB_LIBS = $(DIST)/lib/$(LIB_PREFIX)zlib.$(LIB_SUFFIX)
endif

JAR_LIBS = $(DIST)/lib/$(LIB_PREFIX)jar.$(LIB_SUFFIX) \
	$(ZLIB_LIBS) \
	$(NULL)
