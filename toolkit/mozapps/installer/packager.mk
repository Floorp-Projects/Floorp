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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Benjamin Smedberg <bsmedberg@covad.net>
#   Arthur Wiebe <artooro@gmail.com>
#   Mark Mentovai <mark@moxienet.com>
#   Robert Strong <robert.bugzilla@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

include $(MOZILLA_DIR)/toolkit/mozapps/installer/package-name.mk

# This is how we create the binary packages we release to the public.

ifndef MOZ_PKG_FORMAT
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
MOZ_PKG_FORMAT  = DMG
else
ifeq (,$(filter-out OS2 WINNT, $(OS_ARCH)))
MOZ_PKG_FORMAT  = ZIP
else
ifeq (,$(filter-out SunOS, $(OS_ARCH)))
   MOZ_PKG_FORMAT  = BZ2
else
   ifeq (,$(filter-out gtk2 qt, $(MOZ_WIDGET_TOOLKIT)))
      MOZ_PKG_FORMAT  = BZ2
   else
      ifeq (Android,$(OS_TARGET))
          MOZ_PKG_FORMAT = APK
      else
          MOZ_PKG_FORMAT = TGZ
      endif
   endif
endif
endif
endif
endif # MOZ_PKG_FORMAT

ifeq ($(OS_ARCH),WINNT)
INSTALLER_DIR   = windows
endif

PACKAGE       = $(PKG_PATH)$(PKG_BASENAME)$(PKG_SUFFIX)

# By default, the SDK uses the same packaging type as the main bundle,
# but on mac it is a .tar.bz2
SDK_PATH      = $(PKG_PATH)
ifeq ($(MOZ_APP_NAME),xulrunner)
SDK_PATH = sdk/
endif
SDK_SUFFIX    = $(PKG_SUFFIX)
SDK           = $(SDK_PATH)$(PKG_BASENAME).sdk$(SDK_SUFFIX)

# JavaScript Shell packaging
ifndef LIBXUL_SDK
JSSHELL_BINS  = \
  $(DIST)/bin/js$(BIN_SUFFIX) \
  $(NULL)
ifndef MOZ_NATIVE_NSPR
JSSHELL_BINS += $(DIST)/bin/$(LIB_PREFIX)nspr4$(DLL_SUFFIX)
ifeq ($(OS_ARCH),WINNT)
ifdef MOZ_MEMORY
JSSHELL_BINS += $(DIST)/bin/jemalloc$(DLL_SUFFIX)
endif
ifeq ($(_MSC_VER),1400)
JSSHELL_BINS += $(DIST)/bin/Microsoft.VC80.CRT.manifest
JSSHELL_BINS += $(DIST)/bin/msvcr80.dll
endif
ifeq ($(_MSC_VER),1500)
JSSHELL_BINS += $(DIST)/bin/Microsoft.VC90.CRT.manifest
JSSHELL_BINS += $(DIST)/bin/msvcr90.dll
endif
ifeq ($(_MSC_VER),1500)
JSSHELL_BINS += $(DIST)/bin/msvcr100.dll
endif
else
JSSHELL_BINS += \
  $(DIST)/bin/$(LIB_PREFIX)plds4$(DLL_SUFFIX) \
  $(DIST)/bin/$(LIB_PREFIX)plc4$(DLL_SUFFIX) \
  $(NULL)
endif
endif # MOZ_NATIVE_NSPR
MAKE_JSSHELL  = $(ZIP) -9j $(PKG_JSSHELL) $(JSSHELL_BINS)
endif # LIBXUL_SDK

MAKE_PACKAGE	= $(error What is a $(MOZ_PKG_FORMAT) package format?);
_ABS_DIST = $(call core_abspath,$(DIST))
JARLOG_DIR = $(call core_abspath,$(DEPTH)/jarlog/)
JARLOG_DIR_AB_CD = $(JARLOG_DIR)/$(AB_CD)

CREATE_FINAL_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode="go-w" -f
UNPACK_TAR       = tar -xf-

ifeq ($(MOZ_PKG_FORMAT),TAR)
PKG_SUFFIX	= .tar
INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) > $(PACKAGE)
INNER_UNMAKE_PACKAGE	= $(UNPACK_TAR) < $(UNPACKAGE)
MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk > $(SDK)
endif
ifeq ($(MOZ_PKG_FORMAT),TGZ)
PKG_SUFFIX	= .tar.gz
INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | gzip -vf9 > $(PACKAGE)
INNER_UNMAKE_PACKAGE	= gunzip -c $(UNPACKAGE) | $(UNPACK_TAR)
MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk | gzip -vf9 > $(SDK)
endif
ifeq ($(MOZ_PKG_FORMAT),BZ2)
PKG_SUFFIX	= .tar.bz2
INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | bzip2 -vf > $(PACKAGE)
INNER_UNMAKE_PACKAGE	= bunzip2 -c $(UNPACKAGE) | $(UNPACK_TAR)
MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk | bzip2 -vf > $(SDK)
endif
ifeq ($(MOZ_PKG_FORMAT),ZIP)
PKG_SUFFIX	= .zip
INNER_MAKE_PACKAGE	= $(ZIP) -r9D $(PACKAGE) $(MOZ_PKG_DIR)
INNER_UNMAKE_PACKAGE	= $(UNZIP) $(UNPACKAGE)
MAKE_SDK = $(ZIP) -r9D $(SDK) $(MOZ_APP_NAME)-sdk
endif
ifeq ($(MOZ_PKG_FORMAT),SFX7Z)
PKG_SUFFIX	= .exe
INNER_MAKE_PACKAGE	= rm -f app.7z && \
  mv $(MOZ_PKG_DIR) core && \
  $(CYGWIN_WRAPPER) 7z a -r -t7z app.7z -mx -m0=BCJ2 -m1=LZMA:d24 \
    -m2=LZMA:d19 -m3=LZMA:d19 -mb0:1 -mb0s1:2 -mb0s2:3 && \
  mv core $(MOZ_PKG_DIR) && \
  cat $(SFX_HEADER) app.7z > $(PACKAGE) && \
  chmod 0755 $(PACKAGE)
INNER_UNMAKE_PACKAGE	= $(CYGWIN_WRAPPER) 7z x $(UNPACKAGE) core && \
  mv core $(MOZ_PKG_DIR)
endif

#Create an RPM file
ifeq ($(MOZ_PKG_FORMAT),RPM)
PKG_SUFFIX  = .rpm
MOZ_NUMERIC_APP_VERSION = $(shell echo $(MOZ_PKG_VERSION) | sed "s/[^0-9.].*//" )
MOZ_RPM_RELEASE = $(shell echo $(MOZ_PKG_VERSION) | sed "s/[0-9.]*//" )

RPMBUILD_TOPDIR=$(_ABS_DIST)/rpmbuild
RPMBUILD_RPMDIR=$(_ABS_DIST)
RPMBUILD_SRPMDIR=$(_ABS_DIST)
RPMBUILD_SOURCEDIR=$(RPMBUILD_TOPDIR)/SOURCES
RPMBUILD_SPECDIR=$(topsrcdir)/toolkit/mozapps/installer/linux/rpm
RPMBUILD_BUILDDIR=$(_ABS_DIST)/..

SPEC_FILE = $(RPMBUILD_SPECDIR)/mozilla.spec
RPM_INCIDENTALS=$(topsrcdir)/toolkit/mozapps/installer/linux/rpm

RPM_CMD = \
  echo Creating RPM && \
  mkdir -p $(RPMBUILD_SOURCEDIR) && \
  $(PYTHON) $(topsrcdir)/config/Preprocessor.py \
	-DMOZ_APP_NAME=$(MOZ_APP_NAME) \
	-DMOZ_APP_DISPLAYNAME=$(MOZ_APP_DISPLAYNAME) \
	< $(RPM_INCIDENTALS)/mozilla.desktop \
	> $(RPMBUILD_SOURCEDIR)/$(MOZ_APP_NAME).desktop && \
  rm -rf $(_ABS_DIST)/$(TARGET_CPU) && \
  $(RPMBUILD) -bb \
  $(SPEC_FILE) \
  --target $(TARGET_CPU) \
  --buildroot $(RPMBUILD_TOPDIR)/BUILDROOT \
  --define "moz_app_name $(MOZ_APP_NAME)" \
  --define "moz_app_displayname $(MOZ_APP_DISPLAYNAME)" \
  --define "moz_app_version $(MOZ_APP_VERSION)" \
  --define "moz_numeric_app_version $(MOZ_NUMERIC_APP_VERSION)" \
  --define "moz_rpm_release $(MOZ_RPM_RELEASE)" \
  --define "buildid $(BUILDID)" \
  --define "moz_source_repo $(MOZ_SOURCE_REPO)" \
  --define "moz_source_stamp $(MOZ_SOURCE_STAMP)" \
  --define "moz_branding_directory $(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)" \
  --define "_topdir $(RPMBUILD_TOPDIR)" \
  --define "_rpmdir $(RPMBUILD_RPMDIR)" \
  --define "_sourcedir $(RPMBUILD_SOURCEDIR)" \
  --define "_specdir $(RPMBUILD_SPECDIR)" \
  --define "_srcrpmdir $(RPMBUILD_SRPMDIR)" \
  --define "_builddir $(RPMBUILD_BUILDDIR)" \
  --define "_prefix $(prefix)" \
  --define "_libdir $(libdir)" \
  --define "_bindir $(bindir)" \
  --define "_datadir $(datadir)" \
  --define "_installdir $(installdir)" 

ifdef ENABLE_TESTS
RPM_CMD += \
  --define "createtests yes" \
  --define "_testsinstalldir $(shell basename $(installdir))" 
endif 

ifdef INSTALL_SDK
RPM_CMD += \
  --define "createdevel yes" \
  --define "_idldir $(idldir)" \
  --define "_sdkdir $(sdkdir)" \
  --define "_includedir $(includedir)" 
endif

#For each of the main, tests, sdk rpms we want to make sure that
#if they exist that they are in objdir/dist/ and that they get 
#uploaded and that they are beside the other build artifacts
MAIN_RPM= $(MOZ_APP_NAME)-$(MOZ_NUMERIC_APP_VERSION)-$(MOZ_RPM_RELEASE).$(BUILDID).$(TARGET_CPU)$(PKG_SUFFIX)
UPLOAD_EXTRA_FILES += $(MAIN_RPM)
RPM_CMD += && mv $(TARGET_CPU)/$(MAIN_RPM) $(_ABS_DIST)/

ifdef ENABLE_TESTS
TESTS_RPM=$(MOZ_APP_NAME)-tests-$(MOZ_NUMERIC_APP_VERSION)-$(MOZ_RPM_RELEASE).$(BUILDID).$(TARGET_CPU)$(PKG_SUFFIX)
UPLOAD_EXTRA_FILES += $(TESTS_RPM)
RPM_CMD += && mv $(TARGET_CPU)/$(TESTS_RPM) $(_ABS_DIST)/
endif

ifdef INSTALL_SDK
SDK_RPM=$(MOZ_APP_NAME)-devel-$(MOZ_NUMERIC_APP_VERSION)-$(MOZ_RPM_RELEASE).$(BUILDID).$(TARGET_CPU)$(PKG_SUFFIX)
UPLOAD_EXTRA_FILES += $(SDK_RPM)
RPM_CMD += && mv $(TARGET_CPU)/$(SDK_RPM) $(_ABS_DIST)/
endif

INNER_MAKE_PACKAGE = $(RPM_CMD)
#Avoiding rpm repacks, going to try creating/uploading xpi in rpm files instead
INNER_UNMAKE_PACKAGE = $(error Try using rpm2cpio and cpio)

endif #Create an RPM file


ifeq ($(MOZ_PKG_FORMAT),APK)

# we have custom stuff for Android
MOZ_OMNIJAR =

JAVA_CLASSPATH = $(ANDROID_SDK)/android.jar
include $(topsrcdir)/config/android-common.mk

JARSIGNER ?= echo

DIST_FILES = \
  resources.arsc \
  AndroidManifest.xml \
  chrome \
  components \
  defaults \
  modules \
  hyphenation \
  res \
  lib \
  lib.id \
  extensions \
  application.ini \
  platform.ini \
  greprefs.js \
  browserconfig.properties \
  blocklist.xml \
  chrome.manifest \
  update.locale \
  removed-files \
  $(NULL)

NON_DIST_FILES = \
  classes.dex \
  $(NULL)

UPLOAD_EXTRA_FILES += gecko-unsigned-unaligned.apk

include $(topsrcdir)/ipc/app/defs.mk

DIST_FILES += $(MOZ_CHILD_PROCESS_NAME)

ifdef MOZ_THUMB2
ABI_DIR = armeabi-v7a
else
ABI_DIR = armeabi
endif

PKG_SUFFIX      = .apk
INNER_MAKE_PACKAGE	= \
  make -C ../embedding/android gecko.ap_ && \
  cp ../embedding/android/gecko.ap_ $(_ABS_DIST) && \
  ( cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && \
    rm -rf lib && \
    mkdir -p lib/$(ABI_DIR) && \
    cp lib*.so lib && \
    mv lib/libmozutils.so lib/$(ABI_DIR) && \
    rm -f lib.id && \
    for SOMELIB in lib/*.so ; \
    do \
      printf "`basename $$SOMELIB`:`$(_ABS_DIST)/host/bin/file_id $$SOMELIB`\n" >> lib.id ; \
    done && \
    unzip -o $(_ABS_DIST)/gecko.ap_ && \
    rm $(_ABS_DIST)/gecko.ap_ && \
    $(ZIP) -r9D $(_ABS_DIST)/gecko.ap_ $(DIST_FILES) -x $(NON_DIST_FILES) ) && \
  rm -f $(_ABS_DIST)/gecko.apk && \
  $(APKBUILDER) $(_ABS_DIST)/gecko.apk -v $(APKBUILDER_FLAGS) -z $(_ABS_DIST)/gecko.ap_ -f $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/classes.dex && \
  cp $(_ABS_DIST)/gecko.apk $(_ABS_DIST)/gecko-unsigned-unaligned.apk && \
  $(JARSIGNER) $(_ABS_DIST)/gecko.apk && \
  $(ZIPALIGN) -f -v 4 $(_ABS_DIST)/gecko.apk $(PACKAGE)
INNER_UNMAKE_PACKAGE	= \
  mkdir $(MOZ_PKG_DIR) && \
  cd $(MOZ_PKG_DIR) && \
  $(UNZIP) $(UNPACKAGE) && \
  mv lib/$(ABI_DIR)/*.so . && \
  mv lib/*.so . && \
  rm -rf lib
endif
ifeq ($(MOZ_PKG_FORMAT),DMG)
ifndef _APPNAME
ifdef MOZ_DEBUG
_APPNAME	= $(MOZ_APP_DISPLAYNAME)Debug.app
else
_APPNAME	= $(MOZ_APP_DISPLAYNAME).app
endif
endif
ifndef _BINPATH
_BINPATH	= /$(_APPNAME)/Contents/MacOS
endif # _BINPATH
PKG_SUFFIX	= .dmg
PKG_DMG_FLAGS	=
ifneq (,$(MOZ_PKG_MAC_DSSTORE))
PKG_DMG_FLAGS += --copy "$(MOZ_PKG_MAC_DSSTORE):/.DS_Store"
endif
ifneq (,$(MOZ_PKG_MAC_BACKGROUND))
PKG_DMG_FLAGS += --mkdir /.background --copy "$(MOZ_PKG_MAC_BACKGROUND):/.background"
endif
ifneq (,$(MOZ_PKG_MAC_ICON))
PKG_DMG_FLAGS += --icon "$(MOZ_PKG_MAC_ICON)"
endif
ifneq (,$(MOZ_PKG_MAC_RSRC))
PKG_DMG_FLAGS += --resource "$(MOZ_PKG_MAC_RSRC)"
endif
ifneq (,$(MOZ_PKG_MAC_EXTRA))
PKG_DMG_FLAGS += $(MOZ_PKG_MAC_EXTRA)
endif
_ABS_MOZSRCDIR = $(shell cd $(MOZILLA_DIR) && pwd)
ifdef UNIVERSAL_BINARY
STAGEPATH = universal/
endif
ifndef PKG_DMG_SOURCE
PKG_DMG_SOURCE = $(STAGEPATH)$(MOZ_PKG_DIR)
endif
INNER_MAKE_PACKAGE	= $(_ABS_MOZSRCDIR)/build/package/mac_osx/pkg-dmg \
  --source "$(PKG_DMG_SOURCE)" --target "$(PACKAGE)" \
  --volname "$(MOZ_APP_DISPLAYNAME)" $(PKG_DMG_FLAGS)
INNER_UNMAKE_PACKAGE	= \
  set -ex; \
  rm -rf $(_ABS_DIST)/unpack.tmp; \
  mkdir -p $(_ABS_DIST)/unpack.tmp; \
  $(_ABS_MOZSRCDIR)/build/package/mac_osx/unpack-diskimage $(UNPACKAGE) /tmp/$(MOZ_PKG_APPNAME)-unpack $(_ABS_DIST)/unpack.tmp; \
  rsync -a "$(_ABS_DIST)/unpack.tmp/$(_APPNAME)" $(MOZ_PKG_DIR); \
  test -n "$(MOZ_PKG_MAC_DSSTORE)" && \
    rsync -a "$(_ABS_DIST)/unpack.tmp/.DS_Store" "$(MOZ_PKG_MAC_DSSTORE)"; \
  test -n "$(MOZ_PKG_MAC_BACKGROUND)" && \
    rsync -a "$(_ABS_DIST)/unpack.tmp/.background/`basename "$(MOZ_PKG_MAC_BACKGROUND)"`" "$(MOZ_PKG_MAC_BACKGROUND)"; \
  test -n "$(MOZ_PKG_MAC_ICON)" && \
    rsync -a "$(_ABS_DIST)/unpack.tmp/.VolumeIcon.icns" "$(MOZ_PKG_MAC_ICON)"; \
  rm -rf $(_ABS_DIST)/unpack.tmp; \
  if test -n "$(MOZ_PKG_MAC_RSRC)" ; then \
    cp $(UNPACKAGE) $(MOZ_PKG_APPNAME).tmp.dmg && \
    hdiutil unflatten $(MOZ_PKG_APPNAME).tmp.dmg && \
    { /Developer/Tools/DeRez -skip plst -skip blkx $(MOZ_PKG_APPNAME).tmp.dmg > "$(MOZ_PKG_MAC_RSRC)" || { rm -f $(MOZ_PKG_APPNAME).tmp.dmg && false; }; } && \
    rm -f $(MOZ_PKG_APPNAME).tmp.dmg; \
  fi
# The plst and blkx resources are skipped because they belong to each
# individual dmg and are created by hdiutil.
SDK_SUFFIX = .tar.bz2
SDK = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).$(AB_CD).mac-$(TARGET_CPU).sdk$(SDK_SUFFIX)
ifeq ($(MOZ_APP_NAME),xulrunner)
SDK = $(SDK_PATH)$(MOZ_APP_NAME)-$(MOZ_PKG_VERSION).$(AB_CD).mac-$(TARGET_CPU).sdk$(SDK_SUFFIX)
endif
MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk | bzip2 -vf > $(SDK)
endif

ifdef MOZ_OMNIJAR

ifdef GENERATE_CACHE
ifneq (1_,$(if $(CROSS_COMPILE),1,0)_$(UNIVERSAL_BINARY))
ifdef RUN_TEST_PROGRAM
_ABS_RUN_TEST_PROGRAM = $(call core_abspath,$(RUN_TEST_PROGRAM))
endif

ifdef LIBXUL_SDK
PRECOMPILE_DIR=XCurProcD
PRECOMPILE_RESOURCE=app
PRECOMPILE_GRE=$(LIBXUL_DIST)/bin
else
PRECOMPILE_DIR=GreD
PRECOMPILE_RESOURCE=gre
PRECOMPILE_GRE=$$PWD
endif

GENERATE_CACHE = \
  $(_ABS_RUN_TEST_PROGRAM) $(LIBXUL_DIST)/bin/xpcshell$(BIN_SUFFIX) -g "$(PRECOMPILE_GRE)" -a "$$PWD" -f $(MOZILLA_DIR)/toolkit/mozapps/installer/precompile_cache.js -e "populate_startupcache('$(PRECOMPILE_DIR)', 'omni.jar', 'startupCache.zip');" && \
  rm -rf jsloader && \
  $(UNZIP) startupCache.zip && \
  rm startupCache.zip && \
  $(ZIP) -r9m omni.jar jsloader/resource/$(PRECOMPILE_RESOURCE)
endif
endif

GENERATE_CACHE ?= true

OMNIJAR_FILES	= \
  chrome \
  chrome.manifest \
  components/*.js \
  components/*.xpt \
  components/*.manifest \
  modules \
  res \
  defaults \
  greprefs.js \
  jsloader \
  $(NULL)

NON_OMNIJAR_FILES += \
  chrome/icons/\* \
  $(PREF_DIR)/channel-prefs.js \
  res/cursors/\* \
  res/MainMenu.nib/\* \
  $(NULL)

PACK_OMNIJAR	= \
  rm -f omni.jar components/binary.manifest && \
  grep -h '^binary-component' components/*.manifest > binary.manifest ; \
  for m in components/*.manifest; do \
    sed -e 's/^binary-component/\#binary-component/' $$m > tmp.manifest && \
    mv tmp.manifest $$m; \
  done; \
  $(ZIP) -r9m omni.jar $(OMNIJAR_FILES) -x $(NON_OMNIJAR_FILES) && \
  $(GENERATE_CACHE) && \
  $(OPTIMIZE_JARS_CMD) --optimize $(JARLOG_DIR_AB_CD) ./ ./ && \
  mv binary.manifest components && \
  printf "manifest components/binary.manifest\n" > chrome.manifest
UNPACK_OMNIJAR	= \
  $(OPTIMIZE_JARS_CMD) --deoptimize $(JARLOG_DIR_AB_CD) ./ ./ && \
  $(UNZIP) -o omni.jar && \
  rm -f components/binary.manifest && \
  for m in components/*.manifest; do \
    sed -e 's/^\#binary-component/binary-component/' $$m > tmp.manifest && \
    mv tmp.manifest $$m; \
  done

MAKE_PACKAGE	= (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && $(PACK_OMNIJAR)) && \
	              (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && $(CREATE_PRECOMPLETE_CMD)) && $(INNER_MAKE_PACKAGE)
UNMAKE_PACKAGE	= $(INNER_UNMAKE_PACKAGE) && (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && $(UNPACK_OMNIJAR))
else
MAKE_PACKAGE	= (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && $(CREATE_PRECOMPLETE_CMD)) && $(INNER_MAKE_PACKAGE)
UNMAKE_PACKAGE	= $(INNER_UNMAKE_PACKAGE)
endif

# dummy macro if we don't have PSM built
SIGN_NSS		=
ifneq (1_,$(if $(CROSS_COMPILE),1,0)_$(UNIVERSAL_BINARY))
ifdef MOZ_PSM
SIGN_NSS		= @echo signing nss libraries;

NSS_DLL_SUFFIX	= $(DLL_SUFFIX)
ifdef UNIVERSAL_BINARY
NATIVE_ARCH	= $(shell uname -p | sed -e s/powerpc/ppc/)
NATIVE_DIST	= $(DIST:$(DEPTH)/%=$(DEPTH)/../$(NATIVE_ARCH)/%)
SIGN_CMD	= $(NATIVE_DIST)/bin/run-mozilla.sh $(NATIVE_DIST)/bin/shlibsign -v -i
else
ifeq ($(OS_ARCH),OS2)
# uppercase extension to get the correct output file from shlibsign
NSS_DLL_SUFFIX	= .DLL
SIGN_CMD	= $(MOZILLA_DIR)/toolkit/mozapps/installer/os2/sign.cmd $(DIST)
else
SIGN_CMD	= $(RUN_TEST_PROGRAM) $(DIST)/bin/shlibsign$(BIN_SUFFIX) -v -i
endif
endif

SOFTOKN		= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)softokn3$(NSS_DLL_SUFFIX)
NSSDBM		= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)nssdbm3$(NSS_DLL_SUFFIX)
FREEBL		= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)freebl3$(NSS_DLL_SUFFIX)
FREEBL_32FPU	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)freebl_32fpu_3$(DLL_SUFFIX)
FREEBL_32INT	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)freebl_32int_3$(DLL_SUFFIX)
FREEBL_32INT64	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)freebl_32int64_3$(DLL_SUFFIX)
FREEBL_64FPU	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)freebl_64fpu_3$(DLL_SUFFIX)
FREEBL_64INT	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(DLL_PREFIX)freebl_64int_3$(DLL_SUFFIX)

SIGN_NSS	+= $(SIGN_CMD) $(SOFTOKN); \
	$(SIGN_CMD) $(NSSDBM); \
	if test -f $(FREEBL); then $(SIGN_CMD) $(FREEBL); fi; \
	if test -f $(FREEBL_32FPU); then $(SIGN_CMD) $(FREEBL_32FPU); fi; \
	if test -f $(FREEBL_32INT); then $(SIGN_CMD) $(FREEBL_32INT); fi; \
	if test -f $(FREEBL_32INT64); then $(SIGN_CMD) $(FREEBL_32INT64); fi; \
	if test -f $(FREEBL_64FPU); then $(SIGN_CMD) $(FREEBL_64FPU); fi; \
	if test -f $(FREEBL_64INT); then $(SIGN_CMD) $(FREEBL_64INT); fi;

endif # MOZ_PSM
endif # !CROSS_COMPILE

NO_PKG_FILES += \
	core \
	bsdecho \
	js \
	js-config \
	jscpucfg \
	nsinstall \
	viewer \
	TestGtkEmbed \
	codesighs* \
	elf-dynstr-gc \
	mangle* \
	maptsv* \
	mfc* \
	mkdepend* \
	msdump* \
	msmap* \
	nm2tsv* \
	nsinstall* \
	res/samples \
	res/throbber \
	shlibsign* \
	ssltunnel* \
	certutil* \
	pk12util* \
	winEmbed.exe \
	chrome/chrome.rdf \
	chrome/app-chrome.manifest \
	chrome/overlayinfo \
	components/compreg.dat \
	components/xpti.dat \
	content_unit_tests \
	necko_unit_tests \
	*.dSYM \
	$(NULL)

# browser/locales/Makefile uses this makefile for its variable defs, but
# doesn't want the libs:: rule.
ifndef PACKAGER_NO_LIBS
libs:: make-package
endif

DEFINES += -DDLL_PREFIX=$(DLL_PREFIX) -DDLL_SUFFIX=$(DLL_SUFFIX) -DBIN_SUFFIX=$(BIN_SUFFIX)

ifdef MOZ_PKG_REMOVALS
MOZ_PKG_REMOVALS_GEN = removed-files

$(MOZ_PKG_REMOVALS_GEN): $(MOZ_PKG_REMOVALS) $(GLOBAL_DEPS)
	cat $(MOZ_PKG_REMOVALS) | \
	sed -e 's/^[ \t]*//' | \
	$(PYTHON) $(MOZILLA_DIR)/config/Preprocessor.py -Fsubstitution $(DEFINES) $(ACDEFINES) > $(MOZ_PKG_REMOVALS_GEN)

GARBAGE += $(MOZ_PKG_REMOVALS_GEN)
endif

GARBAGE		+= $(DIST)/$(PACKAGE) $(PACKAGE)

ifeq ($(OS_ARCH),IRIX)
STRIP_FLAGS	= -f
endif
ifeq ($(OS_ARCH),OS2)
STRIP		= $(MOZILLA_DIR)/toolkit/mozapps/installer/os2/strip.cmd
endif

ifneq (,$(filter WINNT OS2,$(OS_ARCH)))
PKGCP_OS = dos
else
PKGCP_OS = unix
endif

# The following target stages files into two directories: one directory for
# core files, and one for optional extensions based on the information in
# the MOZ_PKG_MANIFEST file and the following vars:
# MOZ_NONLOCALIZED_PKG_LIST
# MOZ_LOCALIZED_PKG_LIST
# MOZ_OPTIONAL_PKG_LIST

PKG_ARG = , "$(pkg)"

# Define packager macro to work around make 3.81 backslash issue (bug #339933)
define PACKAGER_COPY
$(PERL) -I$(MOZILLA_DIR)/xpinstall/packager -e 'use Packager; \
       Packager::Copy($1,$2,$3,$4,$5,$6,$7);'
endef

installer-stage: stage-package
ifndef MOZ_PKG_MANIFEST
	$(error MOZ_PKG_MANIFEST unspecified!)
endif
	@rm -rf $(DEPTH)/installer-stage $(DIST)/xpt
	@echo "Staging installer files..."
	@$(NSINSTALL) -D $(DEPTH)/installer-stage/core
ifdef MOZ_OMNIJAR
	@(cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && $(PACK_OMNIJAR))
endif
	@cp -av $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/. $(DEPTH)/installer-stage/core
	@(cd $(DEPTH)/installer-stage/core && $(CREATE_PRECOMPLETE_CMD))
ifdef MOZ_OPTIONAL_PKG_LIST
	@$(NSINSTALL) -D $(DEPTH)/installer-stage/optional
	$(call PACKAGER_COPY, "$(call core_abspath,$(DIST))",\
	  "$(call core_abspath,$(DEPTH)/installer-stage/optional)", \
	  "$(MOZ_PKG_MANIFEST)", "$(PKGCP_OS)", 1, 0, 1 \
	  $(foreach pkg,$(MOZ_OPTIONAL_PKG_LIST),$(PKG_ARG)) )
	if test -d $(DEPTH)/installer-stage/optional/extensions ; then \
		cd $(DEPTH)/installer-stage/optional/extensions; find -maxdepth 1 -mindepth 1 -exec rm -r ../../core/extensions/{} \; ; \
	fi
	if test -d $(DEPTH)/installer-stage/optional/distribution/extensions/ ; then \
		cd $(DEPTH)/installer-stage/optional/distribution/extensions/; find -maxdepth 1 -mindepth 1 -exec rm -r ../../../core/distribution/extensions/{} \; ; \
	fi
endif

elfhack:
ifdef USE_ELF_HACK
	@echo ===
	@echo === If you get failures below, please file a bug describing the error
	@echo === and your environment \(compiler and linker versions\), and use
	@echo === --disable-elf-hack until this is fixed.
	@echo ===
	cd $(DIST)/bin; find . -name "*$(DLL_SUFFIX)" | xargs $(DEPTH)/build/unix/elfhack/elfhack
endif

stage-package: $(MOZ_PKG_MANIFEST) $(MOZ_PKG_REMOVALS_GEN) elfhack
	@rm -rf $(DIST)/$(MOZ_PKG_DIR) $(DIST)/$(PKG_PATH)$(PKG_BASENAME).tar $(DIST)/$(PKG_PATH)$(PKG_BASENAME).dmg $@ $(EXCLUDE_LIST)
# NOTE: this must be a tar now that dist links into the tree so that we
# do not strip the binaries actually in the tree.
	@echo "Creating package directory..."
	@mkdir $(DIST)/$(MOZ_PKG_DIR)
ifndef UNIVERSAL_BINARY
# If UNIVERSAL_BINARY, the package will be made from an already-prepared
# STAGEPATH
ifdef MOZ_PKG_MANIFEST
	$(RM) -rf $(DIST)/xpt $(DIST)/manifests
	$(call PACKAGER_COPY, "$(call core_abspath,$(DIST))",\
	  "$(call core_abspath,$(DIST)/$(MOZ_PKG_DIR))", \
	  "$(MOZ_PKG_MANIFEST)", "$(PKGCP_OS)", 1, 0, 1)
	$(PERL) $(MOZILLA_DIR)/xpinstall/packager/xptlink.pl -s $(DIST) -d $(DIST)/xpt -f $(DIST)/$(MOZ_PKG_DIR)/$(_BINPATH)/components -v -x "$(XPIDL_LINK)"
	$(PYTHON) $(MOZILLA_DIR)/toolkit/mozapps/installer/link-manifests.py \
	  $(DIST)/$(MOZ_PKG_DIR)/$(_BINPATH)/components/components.manifest \
	  $(patsubst %,$(DIST)/manifests/%/components,$(MOZ_NONLOCALIZED_PKG_LIST))
	$(PYTHON) $(MOZILLA_DIR)/toolkit/mozapps/installer/link-manifests.py \
	  $(DIST)/$(MOZ_PKG_DIR)/$(_BINPATH)/chrome/nonlocalized.manifest \
	  $(patsubst %,$(DIST)/manifests/%/chrome,$(MOZ_NONLOCALIZED_PKG_LIST))
	$(PYTHON) $(MOZILLA_DIR)/toolkit/mozapps/installer/link-manifests.py \
	  $(DIST)/$(MOZ_PKG_DIR)/$(_BINPATH)/chrome/localized.manifest \
	  $(patsubst %,$(DIST)/manifests/%/chrome,$(MOZ_LOCALIZED_PKG_LIST))
	printf "manifest components/interfaces.manifest\nmanifest components/components.manifest\nmanifest chrome/nonlocalized.manifest\nmanifest chrome/localized.manifest\n" > $(DIST)/$(MOZ_PKG_DIR)/$(_BINPATH)/chrome.manifest
else # !MOZ_PKG_MANIFEST
ifeq ($(MOZ_PKG_FORMAT),DMG)
ifndef STAGE_SDK
	@cd $(DIST) && rsync -auv --copy-unsafe-links $(_APPNAME) $(MOZ_PKG_DIR)
	@echo "Linking XPT files..."
	@rm -rf $(DIST)/xpt
	@$(NSINSTALL) -D $(DIST)/xpt
	@($(XPIDL_LINK) $(DIST)/xpt/$(MOZ_PKG_APPNAME).xpt $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/components/*.xpt && rm -f $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/components/*.xpt && cp $(DIST)/xpt/$(MOZ_PKG_APPNAME).xpt $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/components && printf "interfaces $(MOZ_PKG_APPNAME).xpt\n" >$(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/components/interfaces.manifest) || echo No *.xpt files found in: $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/components/.  Continuing...
else
	@cd $(DIST)/bin && tar $(TAR_CREATE_FLAGS) - * | (cd ../$(MOZ_PKG_DIR); tar -xf -)
endif
else
	@cd $(DIST)/bin && tar $(TAR_CREATE_FLAGS) - * | (cd ../$(MOZ_PKG_DIR); tar -xf -)
	@echo "Linking XPT files..."
	@rm -rf $(DIST)/xpt
	@$(NSINSTALL) -D $(DIST)/xpt
	@($(XPIDL_LINK) $(DIST)/xpt/$(MOZ_PKG_APPNAME).xpt $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)/components/*.xpt && rm -f $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)/components/*.xpt && cp $(DIST)/xpt/$(MOZ_PKG_APPNAME).xpt $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)/components && printf "interfaces $(MOZ_PKG_APPNAME).xpt\n" >$(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)/components/interfaces.manifest) || echo No *.xpt files found in: $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)/components/.  Continuing...
endif # DMG
endif # MOZ_PKG_MANIFEST
endif # UNIVERSAL_BINARY
	$(OPTIMIZE_JARS_CMD) --optimize $(JARLOG_DIR_AB_CD) $(DIST)/bin/chrome $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/chrome
ifndef PKG_SKIP_STRIP
  ifeq ($(OS_ARCH),OS2)
		@echo "Stripping package directory..."
		@cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR) && $(STRIP)
		$(SIGN_NSS)
  else
		@echo "Stripping package directory..."
		@cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR); find . ! -type d \
				! -name "*.js" \
				! -name "*.xpt" \
				! -name "*.gif" \
				! -name "*.jpg" \
				! -name "*.png" \
				! -name "*.xpm" \
				! -name "*.txt" \
				! -name "*.rdf" \
				! -name "*.sh" \
				! -name "*.properties" \
				! -name "*.dtd" \
				! -name "*.html" \
				! -name "*.xul" \
				! -name "*.css" \
				! -name "*.xml" \
				! -name "*.jar" \
				! -name "*.dat" \
				! -name "*.tbl" \
				! -name "*.src" \
				! -name "*.reg" \
				$(PLATFORM_EXCLUDE_LIST) \
				-exec $(STRIP) $(STRIP_FLAGS) {} >/dev/null 2>&1 \;
		$(SIGN_NSS)
  endif
else
ifdef UNIVERSAL_BINARY
# universal binaries will have had their .chk files removed prior to the unify
# step, and if they're also --disable-install-strip then they won't get
# re-signed in the block above.
	$(SIGN_NSS)
endif # UNIVERSAL_BINARY
endif # PKG_SKIP_STRIP
	@echo "Removing unpackaged files..."
ifdef NO_PKG_FILES
	cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH); rm -rf $(NO_PKG_FILES)
endif
ifdef MOZ_PKG_REMOVALS
	$(SYSINSTALL) $(IFLAGS1) $(MOZ_PKG_REMOVALS_GEN) $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)
endif # MOZ_PKG_REMOVALS
ifdef MOZ_POST_STAGING_CMD
	cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && $(MOZ_POST_STAGING_CMD)
endif # MOZ_POST_STAGING_CMD
ifndef LIBXUL_SDK
# Package JavaScript Shell
	@echo "Packaging JavaScript Shell..."
	$(RM) $(PKG_JSSHELL)
	$(MAKE_JSSHELL)
endif # LIBXUL_SDK

make-package: stage-package $(PACKAGE_XULRUNNER) make-sourcestamp-file
	@echo "Compressing..."
	cd $(DIST) && $(MAKE_PACKAGE)

make-sourcestamp-file::
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
	@echo "$(BUILDID)" > $(MOZ_SOURCESTAMP_FILE)
	@echo "$(MOZ_SOURCE_REPO)/rev/$(MOZ_SOURCE_STAMP)" >> $(MOZ_SOURCESTAMP_FILE)

# The install target will install the application to prefix/lib/appname-version
# In addition if INSTALL_SDK is set, it will install the development headers,
# libraries, and IDL files as follows:
# dist/include -> prefix/include/appname-version
# dist/idl -> prefix/share/idl/appname-version
# dist/sdk/lib -> prefix/lib/appname-devel-version/lib
# prefix/lib/appname-devel-version/* symlinks to the above directories
install:: stage-package
ifeq ($(OS_ARCH),WINNT)
	$(error "make install" is not supported on this platform. Use "make package" instead.)
endif
ifeq (bundle,$(MOZ_FS_LAYOUT))
	$(error "make install" is not supported on this platform. Use "make package" instead.)
endif
ifdef MOZ_OMNIJAR
	cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && $(PACK_OMNIJAR)
endif
	$(NSINSTALL) -D $(DESTDIR)$(installdir)
	(cd $(DIST)/$(MOZ_PKG_DIR) && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DESTDIR)$(installdir) && tar -xf -)
	$(NSINSTALL) -D $(DESTDIR)$(bindir)
	$(RM) -f $(DESTDIR)$(bindir)/$(MOZ_APP_NAME)
	ln -s $(installdir)/$(MOZ_APP_NAME) $(DESTDIR)$(bindir)
ifdef INSTALL_SDK # Here comes the hard part
	$(NSINSTALL) -D $(DESTDIR)$(includedir)
	(cd $(DIST)/include && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DESTDIR)$(includedir) && tar -xf -)
	$(NSINSTALL) -D $(DESTDIR)$(idldir)
	(cd $(DIST)/idl && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DESTDIR)$(idldir) && tar -xf -)
# SDK directory is the libs + a bunch of symlinks
	$(NSINSTALL) -D $(DESTDIR)$(sdkdir)/sdk/lib
	$(NSINSTALL) -D $(DESTDIR)$(sdkdir)/sdk/bin
	if test -f $(DIST)/include/xpcom-config.h; then \
	  $(SYSINSTALL) $(IFLAGS1) $(DIST)/include/xpcom-config.h $(DESTDIR)$(sdkdir); \
	fi
	(cd $(DIST)/sdk/lib && tar $(TAR_CREATE_FLAGS) - .) | (cd $(DESTDIR)$(sdkdir)/sdk/lib && tar -xf -)
	(cd $(DIST)/sdk/bin && tar $(TAR_CREATE_FLAGS) - .) | (cd $(DESTDIR)$(sdkdir)/sdk/bin && tar -xf -)
	$(RM) -f $(DESTDIR)$(sdkdir)/lib $(DESTDIR)$(sdkdir)/bin $(DESTDIR)$(sdkdir)/include $(DESTDIR)$(sdkdir)/include $(DESTDIR)$(sdkdir)/sdk/idl $(DESTDIR)$(sdkdir)/idl
	ln -s $(sdkdir)/sdk/lib $(DESTDIR)$(sdkdir)/lib
	ln -s $(installdir) $(DESTDIR)$(sdkdir)/bin
	ln -s $(includedir) $(DESTDIR)$(sdkdir)/include
	ln -s $(idldir) $(DESTDIR)$(sdkdir)/idl
endif # INSTALL_SDK

make-sdk:
	$(MAKE) stage-package UNIVERSAL_BINARY= STAGE_SDK=1 MOZ_PKG_DIR=sdk-stage
	@echo "Packaging SDK..."
	$(RM) -rf $(DIST)/$(MOZ_APP_NAME)-sdk
	$(NSINSTALL) -D $(DIST)/$(MOZ_APP_NAME)-sdk/bin
	(cd $(DIST)/sdk-stage && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DIST)/$(MOZ_APP_NAME)-sdk/bin && tar -xf -)
	$(NSINSTALL) -D $(DIST)/$(MOZ_APP_NAME)-sdk/host/bin
	(cd $(DIST)/host/bin && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DIST)/$(MOZ_APP_NAME)-sdk/host/bin && tar -xf -)
	$(NSINSTALL) -D $(DIST)/$(MOZ_APP_NAME)-sdk/sdk
	(cd $(DIST)/sdk && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DIST)/$(MOZ_APP_NAME)-sdk/sdk && tar -xf -)
	$(NSINSTALL) -D $(DIST)/$(MOZ_APP_NAME)-sdk/include
	(cd $(DIST)/include && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DIST)/$(MOZ_APP_NAME)-sdk/include && tar -xf -)
	$(NSINSTALL) -D $(DIST)/$(MOZ_APP_NAME)-sdk/idl
	(cd $(DIST)/idl && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DIST)/$(MOZ_APP_NAME)-sdk/idl && tar -xf -)
	$(NSINSTALL) -D $(DIST)/$(MOZ_APP_NAME)-sdk/lib
# sdk/lib is the same as sdk/sdk/lib
	(cd $(DIST)/sdk/lib && tar $(TAR_CREATE_FLAGS) - .) | \
	  (cd $(DIST)/$(MOZ_APP_NAME)-sdk/lib && tar -xf -)
	$(NSINSTALL) -D $(DIST)/$(SDK_PATH)
	cd $(DIST) && $(MAKE_SDK)

ifeq ($(OS_TARGET), WINNT)
INSTALLER_PACKAGE = $(DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe
endif

# These are necessary because some of our packages/installers contain spaces
# in their filenames and GNU Make's $(wildcard) function doesn't properly
# deal with them.
empty :=
space = $(empty) $(empty)
QUOTED_WILDCARD = $(if $(wildcard $(subst $(space),?,$(1))),"$(1)")
ESCAPE_SPACE = $(subst $(space),\$(space),$(1))

# This variable defines which OpenSSL algorithm to use to 
# generate checksums for files that we upload
CHECKSUM_ALGORITHM = 'sha512'

# This variable defines where the checksum file will be located
CHECKSUM_FILE = "$(DIST)/$(PKG_PATH)/$(PKG_BASENAME).checksums"

UPLOAD_FILES= \
  $(call QUOTED_WILDCARD,$(DIST)/$(PACKAGE)) \
  $(call QUOTED_WILDCARD,$(INSTALLER_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(COMPLETE_MAR)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(LANGPACK)) \
  $(call QUOTED_WILDCARD,$(wildcard $(DIST)/$(PARTIAL_MAR))) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(TEST_PACKAGE)) \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(SYMBOL_ARCHIVE_BASENAME).zip) \
  $(call QUOTED_WILDCARD,$(DIST)/$(SDK)) \
  $(call QUOTED_WILDCARD,$(MOZ_SOURCESTAMP_FILE)) \
  $(call QUOTED_WILDCARD,$(PKG_JSSHELL)) \
  $(if $(UPLOAD_EXTRA_FILES), $(foreach f, $(UPLOAD_EXTRA_FILES), $(wildcard $(DIST)/$(f))))

checksum:
	mkdir -p `dirname $(CHECKSUM_FILE)`
	@$(PYTHON) $(MOZILLA_DIR)/build/checksums.py \
		-o $(CHECKSUM_FILE) \
		-d $(CHECKSUM_ALGORITHM) \
		-s $(call QUOTED_WILDCARD,$(DIST)) \
		$(UPLOAD_FILES)
	@echo "CHECKSUM FILE START"
	@cat $(CHECKSUM_FILE)
	@echo "CHECKSUM FILE END"


upload: checksum
	$(PYTHON) $(MOZILLA_DIR)/build/upload.py --base-path $(DIST) \
		$(UPLOAD_FILES) \
		$(CHECKSUM_FILE)

ifeq (WINNT,$(OS_TARGET))
CODESIGHS_PACKAGE = $(INSTALLER_PACKAGE)
else
CODESIGHS_PACKAGE = $(DIST)/$(PACKAGE)
endif

codesighs:
	$(PYTHON) $(topsrcdir)/tools/codesighs/codesighs.py \
	  "$(DIST)/$(MOZ_PKG_DIR)" "$(CODESIGHS_PACKAGE)"

ifndef MOZ_PKG_SRCDIR
MOZ_PKG_SRCDIR = $(topsrcdir)
endif

DIR_TO_BE_PACKAGED ?= ../$(notdir $(topsrcdir))
SRC_TAR_EXCLUDE_PATHS += \
  --exclude=".hg*" \
  --exclude="CVS" \
  --exclude=".cvs*" \
  --exclude=".mozconfig*" \
  --exclude="*.pyc" \
  --exclude="$(MOZILLA_DIR)/Makefile" \
  --exclude="$(MOZILLA_DIR)/dist"
ifdef MOZ_OBJDIR
SRC_TAR_EXCLUDE_PATHS += --exclude="$(MOZ_OBJDIR)"
endif
CREATE_SOURCE_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode="go-w" $(SRC_TAR_EXCLUDE_PATHS) -f

# source-package creates a source tarball from the files in MOZ_PKG_SRCDIR,
# which is either set to a clean checkout or defaults to $topsrcdir
source-package:
	@echo "Packaging source tarball..."
	mkdir -p $(DIST)/$(PKG_SRCPACK_PATH)
	(cd $(MOZ_PKG_SRCDIR) && $(CREATE_SOURCE_TAR) - $(DIR_TO_BE_PACKAGED)) | bzip2 -vf > $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_SRCPACK_BASENAME).tar.bz2
