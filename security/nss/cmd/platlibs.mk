#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#


ifdef USE_STATIC_LIBS
# can't do this in manifest.mn because OS_ARCH isn't defined there.
ifeq ($(OS_ARCH), WINNT)

DEFINES += -DNSS_USE_STATIC_LIBS
# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
CRYPTOLIB=$(DIST)/lib/freebl.lib
ifdef MOZILLA_SECURITY_BUILD
	CRYPTOLIB=$(DIST)/lib/crypto.lib
endif
ifdef MOZILLA_BSAFE_BUILD
	CRYPTOLIB+=$(DIST)/lib/bsafe$(BSAFEVER).lib
	CRYPTOLIB+=$(DIST)/lib/freebl.lib
endif

EXTRA_LIBS += \
	$(DIST)/lib/smime.lib \
	$(DIST)/lib/ssl.lib \
	$(DIST)/lib/jar.lib \
	$(DIST)/lib/zlib.lib \
	$(DIST)/lib/nss.lib \
	$(DIST)/lib/ssl.lib \
	$(DIST)/lib/sectool.lib \
	$(DIST)/lib/pkcs12.lib \
	$(DIST)/lib/pkcs7.lib \
	$(DIST)/lib/certhi.lib \
	$(DIST)/lib/cryptohi.lib \
	$(DIST)/lib/pk11wrap.lib \
	$(DIST)/lib/certdb.lib \
	$(DIST)/lib/softokn.lib \
	$(CRYPTOLIB) \
	$(DIST)/lib/swfci.lib \
	$(DIST)/lib/secutil.lib \
	$(DIST)/lib/nsspki.lib \
	$(DIST)/lib/nssdev.lib \
	$(DIST)/lib/nssb.lib \
	$(DIST)/lib/dbm.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(OS_LIBS)
OS_LIBS += \
	wsock32.lib \
	winmm.lib \
	$(NULL)
else

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
CRYPTOLIB=$(DIST)/lib/libfreebl.$(LIB_SUFFIX)
ifdef MOZILLA_SECURITY_BUILD
	CRYPTOLIB=$(DIST)/lib/libcrypto.$(LIB_SUFFIX)
endif
ifdef MOZILLA_BSAFE_BUILD
	CRYPTOLIB+=$(DIST)/lib/libbsafe.$(LIB_SUFFIX)
	CRYPTOLIB+=$(DIST)/lib/libfreebl.$(LIB_SUFFIX)
endif
EXTRA_LIBS += \
	$(DIST)/lib/libsmime.$(LIB_SUFFIX) \
	$(DIST)/lib/libssl.$(LIB_SUFFIX) \
	$(DIST)/lib/libjar.$(LIB_SUFFIX) \
	$(DIST)/lib/libzlib.$(LIB_SUFFIX) \
	$(DIST)/lib/libnss.$(LIB_SUFFIX) \
	$(DIST)/lib/libssl.$(LIB_SUFFIX) \
	$(DIST)/lib/libsectool.$(LIB_SUFFIX) \
	$(DIST)/lib/libpkcs12.$(LIB_SUFFIX) \
	$(DIST)/lib/libpkcs7.$(LIB_SUFFIX) \
	$(DIST)/lib/libcerthi.$(LIB_SUFFIX) \
	$(DIST)/lib/libpk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/libcryptohi.$(LIB_SUFFIX) \
	$(DIST)/lib/libcerthi.$(LIB_SUFFIX) \
	$(DIST)/lib/libnsspki.$(LIB_SUFFIX) \
	$(DIST)/lib/libpk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/libsoftokn.$(LIB_SUFFIX) \
	$(DIST)/lib/libcertdb.$(LIB_SUFFIX) \
	$(DIST)/lib/libnsspki.$(LIB_SUFFIX) \
	$(DIST)/lib/libnssdev.$(LIB_SUFFIX) \
	$(DIST)/lib/libnssb.$(LIB_SUFFIX) \
	$(DIST)/lib/libswfci.$(LIB_SUFFIX) \
	$(CRYPTOLIB) \
	$(DIST)/lib/libsecutil.$(LIB_SUFFIX) \
	$(DIST)/lib/libdbm.$(LIB_SUFFIX) \
	$(NULL)

ifeq ($(OS_ARCH), AIX) 
EXTRA_SHARED_LIBS += -brtl 
endif

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
ifdef XP_OS2_VACPP
EXTRA_SHARED_LIBS += \
	$(DIST)/lib/plc4.lib \
	$(DIST)/lib/plds4.lib \
	$(DIST)/lib/nspr4.lib \
	$(NULL)
else
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib/ \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif
endif

else
# can't do this in manifest.mn because OS_ARCH isn't defined there.
ifeq ($(OS_ARCH), WINNT)

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
EXTRA_LIBS += \
	$(DIST)/lib/sectool.lib \
	$(DIST)/lib/smime3.lib \
	$(DIST)/lib/ssl3.lib \
	$(DIST)/lib/nss3.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(OS_LIBS)
OS_LIBS += \
	wsock32.lib \
	winmm.lib \
	$(NULL)
else

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
EXTRA_LIBS += \
	$(DIST)/lib/libsectool.$(LIB_SUFFIX) \
	$(NULL)

ifeq ($(OS_ARCH), AIX) 
EXTRA_SHARED_LIBS += -brtl 
endif

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib/ \
	-lsoftokn3 \
	-lssl3 \
	-lsmime3 \
	-lnss3 \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif

endif
