#! gmake
#
#           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
#              NETSCAPE COMMUNICATIONS CORPORATION
# Copyright ¨ 1996, 1997 Netscape Communications Corporation.  All Rights
# Reserved.  Use of this Source Code is subject to the terms of the
# applicable license agreement from Netscape Communications Corporation.
# The copyright notice(s) in this Source Code does not indicate actual or
# intended publication of this Source Code.
#

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################

#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

ifdef MOZILLA_SECURITY_BUILD
# LDAP library
LDAPSDK_SNAPSHOT = 19981116D
LDAPSDK_ROOTDIR = components/ldapsdk31

# Linux 2.2 overrides
ifeq ($(OS_ARCH), Linux)
ifeq ($(OS_RELEASE), 2.2)
LDAPSDK_SNAPSHOT = 19991122E
endif
endif

ifdef USE_LOCAL_LDAPSDK
LDAPSDKDIST = $(CORE_DEPTH)/ldapsdk/$(PLATFORM)
else
ifeq ($(OS_ARCH), WINNT)
LDAPSDK_BUILD_DIST = \\\\IRIDIUM/$(LDAPSDK_ROOTDIR)/$(LDAPSDK_SNAPSHOT)/WINNT$(OS_RELEASE)$(OBJDIR_TAG).OBJ
LDAPSDK_RELEASE_DIST = \\\\IRIDIUM/$(LDAPSDK_ROOTDIR)/$(LDAPSDK_SNAPSHOT)/WINNT$(OS_RELEASE)$(OBJDIR_TAG).OBJ
LDAPSDKDIST = $(LDAPSDK_BUILD_DIST)
else
LDAPSDKDIST = /share/builds/$(LDAPSDK_ROOTDIR)/$(LDAPSDK_SNAPSHOT)/$(PLATFORM)
endif
endif
endif

ifneq ($(OS_ARCH), WINNT)
ifeq ($(OS_ARCH), Linux)
# On linux, we link with libstdc++ (we're always using egcs on linux, right?)
CPLUSPLUSRUNTIME = -L /usr/lib -lstdc++ -lm
else
# libC, presumably, is what we must link with elsewhere
CPLUSPLUSRUNTIME = -lC -lm
endif
endif

INCLUDES += -I$(LDAPSDKDIST)/include -I$(CORE_DEPTH)/../dist/public/I18N

ifeq ($(OS_ARCH), SunOS)
ifeq ($(OS_RELEASE), 5.5.1)
OS_LIBS += -ldl -lsocket -lnsl -lthread -lposix4
endif
ifeq ($(OS_RELEASE), 5.6)
OS_LIBS += -ldl -lsocket -lnsl -lthread -lposix4
endif
endif

ifeq ($(OS_ARCH), Linux)
ifdef USE_PTHREADS
# Replace OS_LIBS, because the order of libpthread, libdl, and libc are
# very important. Otherwise you get horrible crashes.
OS_LIBS = -lpthread -ldl -lc
endif
endif

# Adding HP-UX Specific Fixes
ifeq ($(OS_ARCH), HP-UX)
ifdef USE_PTHREADS
OS_LIBS += -lpthread
endif
endif

#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

ifdef NSM_TIMEBOMB
CFLAGS += -DTIMEBOMB
export::
	gmake -f timebomb.mk export libs run
endif


#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################


# can't do this in manifest.mn because OS_ARCH isn't defined there.
ifeq ($(OS_ARCH), WINNT)

NSPR_STUB_LIBS = \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)nspr4.lib

NSPR_DYNAMIC_LIBS = \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4.dll \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4.dll \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)nspr4.dll

NSPR_LIBS = $(NSPR_DYNAMIC_LIBS)

I18N_COMPONENTS = \
	$(DIST)/lib/mimetype.dll \
	$(DIST)/lib/necko.dll \
	$(DIST)/lib/nkfile.dll \
	$(DIST)/lib/nkres.dll \
	$(DIST)/lib/nslocale.dll \
	$(DIST)/lib/strres.dll \
	$(DIST)/lib/ucharuti.dll \
	$(DIST)/lib/uconv.dll \
	$(DIST)/lib/ucvlatin.dll \
	$(NULL)

XPCOM_LIBS = \
	$(DIST)/lib/mozreg.dll \
	$(DIST)/lib/xpcom.dll \
	$(NULL)

EXTRA_LIBS = \
	$(DIST)/lib/ssl.lib \
	$(DIST)/lib/jar.lib \
	$(DIST)/lib/zlib.lib \
	$(DIST)/lib/nss.lib \
	$(DIST)/lib/ssl.lib \
	$(DIST)/lib/crmf.lib \
	$(DIST)/lib/sectool.lib \
	$(DIST)/lib/pkcs12.lib \
	$(DIST)/lib/pkcs7.lib \
	$(DIST)/lib/certhi.lib \
	$(DIST)/lib/pk11wrap.lib \
	$(DIST)/lib/cryptohi.lib \
	$(DIST)/lib/certhi.lib \
	$(DIST)/lib/pk11wrap.lib \
	$(DIST)/lib/softoken.lib \
	$(DIST)/lib/certdb.lib \
	$(DIST)/lib/swfci.lib \
	$(DIST)/lib/crypto.lib \
	$(DIST)/lib/freebl.lib \
	$(DIST)/lib/secutil.lib \
	$(DIST)/lib/crypto.lib \
	$(DIST)/lib/dbm.lib \
	$(DIST)/lib/protocol.lib \
	$(DIST)/lib/xpcom.lib \
	$(NSPR_STUB_LIBS) \
	$(LDAPSDKDIST)/lib/nsldaps32v30.lib \
	wsock32.lib \
	winmm.lib \
	advapi32.lib \
	$(NULL)

else

# Removing Hardcoded shared lib suffixes so that HP-UX Builds
NSPR_LIBS = \
	$(DIST)/lib/libplc4.$(DLL_SUFFIX) \
	$(DIST)/lib/libplds4.$(DLL_SUFFIX) \
	$(DIST)/lib/libnspr4.$(DLL_SUFFIX)

I18N_COMPONENTS = \
	$(DIST)/lib/mimetype.$(DLL_SUFFIX) \
	$(DIST)/lib/necko.$(DLL_SUFFIX) \
	$(DIST)/lib/nkfile.$(DLL_SUFFIX) \
	$(DIST)/lib/nkres.$(DLL_SUFFIX) \
	$(DIST)/lib/nslocale.$(DLL_SUFFIX) \
	$(DIST)/lib/strres.$(DLL_SUFFIX) \
	$(DIST)/lib/ucharuti.$(DLL_SUFFIX) \
	$(DIST)/lib/uconv.$(DLL_SUFFIX) \
	$(DIST)/lib/ucvlatin.$(DLL_SUFFIX) \
	$(NULL)

XPCOM_LIBS = \
	$(DIST)/lib/mozreg.$(DLL_SUFFIX) \
	$(DIST)/lib/xpcom.$(DLL_SUFFIX) \
	$(NULL)

ifeq ($(OS_ARCH), SunOS)
NSPR_LINK_LIBS = -L$(DIST)/lib -Bdynamic -lnspr3 -lplc3 -lplds3
else
NSPR_LINK_LIBS = $(NSPR_LIBS)
endif

# Adding HP-UX Specific Fixes
ifeq ($(OS_ARCH), HP-UX)
NSPR_LINK_LIBS = -L$(DIST)/lib -shared -lnspr3 -lplc3 -lplds3
endif

EXTRA_LIBS += \
        $(DIST)/lib/libssl.a \
        $(DIST)/lib/libjar.a \
        $(DIST)/lib/libzlib.a \
        $(DIST)/lib/libnss.a \
        $(DIST)/lib/libssl.a \
        $(DIST)/lib/libcrmf.a \
        $(DIST)/lib/libsectool.a \
        $(DIST)/lib/libpkcs12.a \
        $(DIST)/lib/libpkcs7.a \
        $(DIST)/lib/libcerthi.a \
        $(DIST)/lib/libpk11wrap.a \
        $(DIST)/lib/libcryptohi.a \
        $(DIST)/lib/libcerthi.a \
        $(DIST)/lib/libpk11wrap.a \
        $(DIST)/lib/libsoftoken.a \
        $(DIST)/lib/libcertdb.a \
        $(DIST)/lib/libswfci.a \
        $(DIST)/lib/libcrypto.a \
        $(DIST)/lib/libfreebl.a \
        $(DIST)/lib/libsecutil.a \
        $(DIST)/lib/libcrypto.a \
        $(DIST)/lib/libdbm.a \
        $(DIST)/lib/libprotocol.a \
        $(DIST)/lib/libxpcom.a \
	$(NSPR_LINK_LIBS) \
	$(CPLUSPLUSRUNTIME) \
	$(LDAPSDKDIST)/lib/libldap30.a \
	$(NULL)

endif

release_md::
	$(NSINSTALL) -m 644 $(NLS_RELEASE_SHLIBS) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
	$(NSINSTALL) -m 644 $(NSPR_LIBS) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm*.properties $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)/ui 
	$(NSINSTALL) -m 644 ../doc/*.htm ../doc/*.html ../doc/*.gif $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)/doc
ifneq ($(OS_ARCH), WINNT)
	$(NSINSTALL) -m 775 start-psm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
endif


install::
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm*.properties $(PLATFORM)/ui
	$(NSINSTALL) -m 644 ../doc/*.gif ../doc/*.html ../doc/*.htm $(PLATFORM)/doc
	$(NSINSTALL) -m 644 $(XPCOM_LIBS) $(PLATFORM)
	$(NSINSTALL) -m 644 $(I18N_COMPONENTS) $(PLATFORM)/components
ifneq ($(OS_ARCH), WINNT)
	$(NSINSTALL) -m 775 start-psm $(PLATFORM)
endif

link:
	if test -f $(PROGRAM); then	\
		echo "rm $(PROGRAM)";	\
		rm $(PROGRAM);		\
	fi;				\
	gmake				\

patch: $(PROGRAM)
#	if test ! -f $(DIST)/bin/plcypatch$(PROG_SUFFIX); then	\
#		gmake import;					\
#	fi;
#	-$(DIST)/bin/plcypatch $(PLCYPATCH_ARGS) $(PROGRAM)
