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

#Which versin of install.js do we really want?
ifeq ($(OS_ARCH), WINNT)
INSTALL_JS = install.js
else 
INSTALL_JS = unix-install.js
endif

# The following don't need a CPLUSPLUSRUNTIME defined:  WINNT, HP-UX
# so filter them out.
ifeq (,$(filter WINNT HP-UX SunOS, $(OS_ARCH)))
ifeq (,$(filter-out Linux FreeBSD, $(OS_ARCH)))
# On linux, we link with libstdc++ (we're always using egcs on linux, right?)
CPLUSPLUSRUNTIME = -L /usr/lib -lstdc++ -lm
else
# libC, presumably, is what we must link with elsewhere
CPLUSPLUSRUNTIME = -lC -lm
endif
endif

ifeq ($(OS_ARCH), SunOS)
ifeq ($(CC), gcc)
CPLUSPLUSRUNTIME = -lstdc++ -lm
else
CPLUSPLUSRUNTIME = -lCrun -lm
endif
endif

INCLUDES += -I$(CORE_DEPTH)/../dist/public/nlslayer -I$(CORE_DEPTH)/../dist/include

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
	$(MAKE) -f timebomb.mk export libs run
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

EXTRA_LIBS = \
	$(DIST)/lib/ssl.lib \
	$(DIST)/lib/nss.lib \
	$(DIST)/lib/ssl.lib \
	$(DIST)/lib/crmf.lib \
	$(DIST)/lib/pkcs12.lib \
	$(DIST)/lib/pkcs7.lib \
	$(DIST)/lib/certhi.lib \
	$(DIST)/lib/pk11wrap.lib \
	$(DIST)/lib/cryptohi.lib \
	$(DIST)/lib/certhi.lib \
	$(DIST)/lib/pk11wrap.lib \
	$(DIST)/lib/softoken.lib \
	$(DIST)/lib/certdb.lib \
	$(DIST)/lib/secutil.lib \
	$(DIST)/lib/freebl.lib \
	$(DIST)/lib/dbm.lib \
	$(DIST)/lib/protocol.lib \
	$(DIST)/lib/nlslayer.lib \
	$(DIST)/lib/xpcom.lib \
	$(NSPR_STUB_LIBS) \
	wsock32.lib \
	winmm.lib \
	advapi32.lib \
	$(NULL)


ifeq ($(BUILD_OPT), 1)
EXTRA_LIBS += $(DIST)/lib/xpcombase_s.lib
endif

else

# Removing Hardcoded shared lib suffixes so that HP-UX Builds
NSPR_LIBS = \
	$(DIST)/lib/libplc4.$(DLL_SUFFIX) \
	$(DIST)/lib/libplds4.$(DLL_SUFFIX) \
	$(DIST)/lib/libnspr4.$(DLL_SUFFIX)

XPCOM_LIBS = \
	$(DIST)/lib/libxpcom.$(DLL_SUFFIX) \
	$(NULL)


ifeq ($(OS_ARCH), SunOS)
ifeq ($(CC), gcc)
NSPR_LINK_LIBS = -L$(DIST)/lib -lnspr4 -lplc4 -lplds4
else
NSPR_LINK_LIBS = -L$(DIST)/lib -Bdynamic -lnspr4 -lplc4 -lplds4
endif
XPCOM_LINK_LIBS = -lxpcom
XPCOM_LIBS += $(DIST)/lib/libz.$(DLL_SUFFIX)
else
NSPR_LINK_LIBS = $(NSPR_LIBS)
XPCOM_LINK_LIBS	= $(XPCOM_LIBS)
endif

ifndef MOZ_DIST
MOZ_DIST=$(DIST)
endif

PROTOCOL_LIBS = -L$(MOZ_DIST)/lib -lprotocol

# Adding HP-UX Specific Fixes
ifeq ($(OS_ARCH), HP-UX)
NSPR_LINK_LIBS = -L$(DIST)/lib -lnspr4 -lplc4 -lplds4
XPCOM_LINK_LIBS = -lxpcom

MKSHLIB = $(CCC) $(DSO_LDOPTS)
MKPROG = $(CCC)
endif

ifeq ($(OS_ARCH),OS2)
NSPR_LINK_LIBS = \
	$(DIST)/lib/plc4.lib \
	$(DIST)/lib/plds4.lib \
	$(DIST)/lib/nspr4.lib

XPCOM_LINK_LIBS = $(DIST)/lib/xpcom.lib
ifdef XP_OS2_EMX
CPLUSPLUSRUNTIME = -lsocket
else
CPLUSPLUSRUNTIME = so32dll.lib tcp32dll.lib
endif
PROTOCOL_LIBS = $(DIST)/lib/libprotocol.lib
endif

EXTRA_LIBS += \
        $(DIST)/lib/libssl.$(LIB_SUFFIX)\
        $(DIST)/lib/libnss.$(LIB_SUFFIX)\
        $(DIST)/lib/libssl.$(LIB_SUFFIX)\
        $(DIST)/lib/libcrmf.$(LIB_SUFFIX)\
        $(DIST)/lib/libpkcs12.$(LIB_SUFFIX)\
        $(DIST)/lib/libpkcs7.$(LIB_SUFFIX)\
        $(DIST)/lib/libcerthi.$(LIB_SUFFIX)\
        $(DIST)/lib/libpk11wrap.$(LIB_SUFFIX)\
        $(DIST)/lib/libcryptohi.$(LIB_SUFFIX)\
        $(DIST)/lib/libcerthi.$(LIB_SUFFIX)\
        $(DIST)/lib/libpk11wrap.$(LIB_SUFFIX)\
        $(DIST)/lib/libsoftoken.$(LIB_SUFFIX)\
        $(DIST)/lib/libcertdb.$(LIB_SUFFIX)\
        $(DIST)/lib/libfreebl.$(LIB_SUFFIX)\
        $(DIST)/lib/libsecutil.$(LIB_SUFFIX)\
        $(DIST)/lib/libdbm.$(LIB_SUFFIX)\
	$(DIST)/lib/libnlslayer.$(LIB_SUFFIX)\
	$(NSPR_LINK_LIBS) \
	$(XPCOM_LINK_LIBS) \
	$(CPLUSPLUSRUNTIME) \
	$(PROTOCOL_LIBS) \
	$(NULL)

endif

release_md::
	$(NSINSTALL) -m 644 $(NSPR_LIBS) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm*.properties $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)/psmdata/ui
	$(NSINSTALL) -m 644 ../doc/*.htm ../doc/*.html ../doc/*.gif $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)/psmdata/doc
ifneq ($(OS_ARCH), WINNT)
	$(NSINSTALL) -m 775 start-psm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
endif


ifndef UI_DIST
UI_DIST = ../ui/$(PLATFORM)
endif

install::
ifeq ($(OS_ARCH),OS2)
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm_bin.properties $(DIST)/bin/psmdata/ui
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm_doc.properties $(DIST)/bin/psmdata/ui
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm_text.properties $(DIST)/bin/psmdata/ui
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm_ui.properties $(DIST)/bin/psmdata/ui
	$(NSINSTALL) -m 644 ../doc/04digsgn.gif $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/06pcrypt.gif $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/bannerrn.gif $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/cartbanner.gif $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/next.gif $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/prev.gif $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/cmcjavascriptapi.html $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/psmtest.html $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/release_notes.html $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/contents.htm $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/glossary.htm $(DIST)/bin/psmdata/doc
	$(NSINSTALL) -m 644 ../doc/help.htm $(DIST)/bin/psmdata/doc
else
	$(NSINSTALL) -m 644 $(UI_DIST)/psm*.properties $(DIST)/bin/psmdata/ui
	$(NSINSTALL) -m 644 ../doc/*.gif ../doc/*.html ../doc/*.htm $(DIST)/bin/psmdata/doc
endif #os2
	$(NSINSTALL) -m 755 $(DIST)/lib/$(DLL_PREFIX)nssckbi.$(DLL_SUFFIX) $(DIST)/bin
ifneq (,$(filter-out OS2 WINNT, $(OS_ARCH)))
	$(NSINSTALL) -m 775 start-psm $(DIST)/bin
endif

link:
	if test -f $(PROGRAM); then	\
		echo "rm $(PROGRAM)";	\
		rm $(PROGRAM);		\
	fi;				\
	$(MAKE)				\

patch: $(PROGRAM)
#	if test ! -f $(DIST)/bin/plcypatch$(PROG_SUFFIX); then	\
#		$(MAKE) import;					\
#	fi;
#	-$(DIST)/bin/plcypatch $(PLCYPATCH_ARGS) $(PROGRAM)

# Files required for .xpi file
#   - psm
#   - nspr libraries
#   - i18n libraries
#   - xpcom libraries
#   - subdirectories (ui, do)

build_xpi: $(PROGRAM)
	$(NSINSTALL) -m 644 ../ui/$(PLATFORM)/psm*.properties xpi/psm/psmdata/ui
	$(NSINSTALL) -m 644 ../doc/*.gif ../doc/*.html ../doc/*.htm xpi/psm/psmdata/doc
	$(NSINSTALL) -m 755 $(XPCOM_LIBS) xpi/psm 
	$(NSINSTALL) -m 755 $(NSPR_LIBS) xpi/psm
ifneq ($(OS_ARCH), WINNT)
	$(NSINSTALL) -m 775 start-psm xpi/psm
endif
	$(NSINSTALL) -m 755 $(PROGRAM) xpi/psm
ifeq ($(OS_ARCH), Linux)
	$(NSINSTALL) -m 755 $(DIST)/bin/regxpcom $(PLATFORM)
	$(NSINSTALL) -m 755 start-regxpcom $(PLATFORM)
	cd $(PLATFORM); start-regxpcom
	$(NSINSTALL) -m 644 $(PLATFORM)/component.reg xpi/psm
endif
	$(NSINSTALL) -m 777 $(PLATFORM)/components/*.* xpi/psm/components
	cp $(INSTALL_JS) xpi/install.js
	cd xpi; zip -T -r ../$(PLATFORM)/psm.xpi psm install.js
	rm -rf xpi
	$(NSINSTALL) -m 644 moz-install.html $(PLATFORM)

release_xpi: build_xpi
	$(NSINSTALL) -m 644 $(PLATFORM)/psm.xpi $(RELEASE_TREE)/$(PLATFORM)/xpi
	$(NSINSTALL) -m 644 moz-install.html $(RELEASE_TREE)/$(PLATFORM)/xpi
