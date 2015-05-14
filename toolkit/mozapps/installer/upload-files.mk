# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef MOZ_PKG_FORMAT
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
MOZ_PKG_FORMAT  = DMG
else
ifeq (,$(filter-out WINNT, $(OS_ARCH)))
MOZ_PKG_FORMAT  = ZIP
else
ifeq (,$(filter-out SunOS, $(OS_ARCH)))
   MOZ_PKG_FORMAT  = BZ2
else
   ifeq (,$(filter-out gtk2 gtk3 qt, $(MOZ_WIDGET_TOOLKIT)))
      MOZ_PKG_FORMAT  = BZ2
   else
      ifeq (android,$(MOZ_WIDGET_TOOLKIT))
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

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifndef _APPNAME
_APPNAME = $(MOZ_MACBUNDLE_NAME)
endif
ifndef _BINPATH
_BINPATH = /$(_APPNAME)/Contents/MacOS
endif # _BINPATH
ifndef _RESPATH
# Resource path for the precomplete file
_RESPATH = /$(_APPNAME)/Contents/Resources
endif
ifdef UNIVERSAL_BINARY
STAGEPATH = universal/
endif
endif

PACKAGE_BASE_DIR = $(_ABS_DIST)
PACKAGE       = $(PKG_PATH)$(PKG_BASENAME)$(PKG_SUFFIX)

# By default, the SDK uses the same packaging type as the main bundle,
# but on mac it is a .tar.bz2
SDK_PATH      = $(PKG_PATH)
ifeq ($(MOZ_APP_NAME),xulrunner)
SDK_PATH = sdk/
# Don't codesign xulrunner internally
MOZ_INTERNAL_SIGNING_FORMAT =
endif
SDK_SUFFIX    = $(PKG_SUFFIX)
SDK           = $(SDK_PATH)$(PKG_BASENAME).sdk$(SDK_SUFFIX)
ifdef UNIVERSAL_BINARY
SDK           = $(SDK_PATH)$(PKG_BASENAME)-$(TARGET_CPU).sdk$(SDK_SUFFIX)
endif

# JavaScript Shell packaging
ifndef LIBXUL_SDK
JSSHELL_BINS  = \
  $(DIST)/bin/js$(BIN_SUFFIX) \
  $(DIST)/bin/$(DLL_PREFIX)mozglue$(DLL_SUFFIX) \
  $(NULL)
ifndef MOZ_NATIVE_NSPR
ifdef MSVC_C_RUNTIME_DLL
JSSHELL_BINS += $(DIST)/bin/$(MSVC_C_RUNTIME_DLL)
endif
ifdef MSVC_CXX_RUNTIME_DLL
JSSHELL_BINS += $(DIST)/bin/$(MSVC_CXX_RUNTIME_DLL)
endif
ifdef MSVC_APPCRT_DLL
JSSHELL_BINS += $(DIST)/bin/$(MSVC_APPCRT_DLL)
endif
ifdef MSVC_DESKTOPCRT_DLL
JSSHELL_BINS += $(DIST)/bin/$(MSVC_DESKTOPCRT_DLL)
endif
ifdef MOZ_FOLD_LIBS
JSSHELL_BINS += $(DIST)/bin/$(DLL_PREFIX)nss3$(DLL_SUFFIX)
else
JSSHELL_BINS += \
  $(DIST)/bin/$(DLL_PREFIX)nspr4$(DLL_SUFFIX) \
  $(DIST)/bin/$(DLL_PREFIX)plds4$(DLL_SUFFIX) \
  $(DIST)/bin/$(DLL_PREFIX)plc4$(DLL_SUFFIX) \
  $(NULL)
endif # MOZ_FOLD_LIBS
endif # MOZ_NATIVE_NSPR
ifdef MOZ_SHARED_ICU
ifeq ($(OS_TARGET), WINNT)
JSSHELL_BINS += \
  $(DIST)/bin/icudt$(MOZ_ICU_DBG_SUFFIX)$(MOZ_ICU_VERSION).dll \
  $(DIST)/bin/icuin$(MOZ_ICU_DBG_SUFFIX)$(MOZ_ICU_VERSION).dll \
  $(DIST)/bin/icuuc$(MOZ_ICU_DBG_SUFFIX)$(MOZ_ICU_VERSION).dll \
  $(NULL)
else
ifeq ($(OS_TARGET), Darwin)
JSSHELL_BINS += \
  $(DIST)/bin/libicudata.$(MOZ_ICU_VERSION).dylib \
  $(DIST)/bin/libicui18n.$(MOZ_ICU_VERSION).dylib \
  $(DIST)/bin/libicuuc.$(MOZ_ICU_VERSION).dylib \
  $(NULL)
else
JSSHELL_BINS += \
  $(DIST)/bin/libicudata.so.$(MOZ_ICU_VERSION) \
  $(DIST)/bin/libicui18n.so.$(MOZ_ICU_VERSION) \
  $(DIST)/bin/libicuuc.so.$(MOZ_ICU_VERSION) \
  $(NULL)
endif # Darwin
endif # WINNT
endif # MOZ_STATIC_JS
MAKE_JSSHELL  = $(PYTHON) $(MOZILLA_DIR)/toolkit/mozapps/installer/dozip.py $(PKG_JSSHELL) $(abspath $(JSSHELL_BINS))
endif # LIBXUL_SDK

_ABS_DIST = $(abspath $(DIST))
JARLOG_DIR = $(abspath $(DEPTH)/jarlog/)
JARLOG_FILE_AB_CD = $(JARLOG_DIR)/$(AB_CD).log

TAR_CREATE_FLAGS := --exclude=.mkdir.done $(TAR_CREATE_FLAGS)
CREATE_FINAL_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode=go-w --exclude=.mkdir.done -f
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
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - -C $(STAGEPATH)$(MOZ_PKG_DIR) $(_APPNAME) | bzip2 -vf > $(PACKAGE)
else
INNER_MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_DIR) | bzip2 -vf > $(PACKAGE)
endif
INNER_UNMAKE_PACKAGE	= bunzip2 -c $(UNPACKAGE) | $(UNPACK_TAR)
MAKE_SDK = $(CREATE_FINAL_TAR) - $(MOZ_APP_NAME)-sdk | bzip2 -vf > $(SDK)
endif
ifeq ($(MOZ_PKG_FORMAT),ZIP)
ifdef MOZ_EXTERNAL_SIGNING_FORMAT
# We can't use signcode on zip files
MOZ_EXTERNAL_SIGNING_FORMAT := $(filter-out osslsigncode signcode,$(MOZ_EXTERNAL_SIGNING_FORMAT))
endif
PKG_SUFFIX	= .zip
INNER_MAKE_PACKAGE	= $(ZIP) -r9D $(PACKAGE) $(MOZ_PKG_DIR) \
  -x \*/.mkdir.done
INNER_UNMAKE_PACKAGE	= $(UNZIP) $(UNPACKAGE)
MAKE_SDK = $(ZIP) -r9D $(SDK) $(MOZ_APP_NAME)-sdk
endif
ifeq ($(MOZ_PKG_FORMAT),SFX7Z)
PKG_SUFFIX	= .exe
INNER_MAKE_PACKAGE	= rm -f app.7z && \
  mv $(MOZ_PKG_DIR) core && \
  $(CYGWIN_WRAPPER) 7z a -r -t7z app.7z -mx -m0=BCJ2 -m1=LZMA:d25 \
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
MOZ_NUMERIC_APP_VERSION = $(shell echo $(MOZ_PKG_VERSION) | sed 's/[^0-9.].*//' )
MOZ_RPM_RELEASE = $(shell echo $(MOZ_PKG_VERSION) | sed 's/[0-9.]*//' )

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
  $(PYTHON) -m mozbuild.action.preprocessor \
	-DMOZ_APP_NAME=$(MOZ_APP_NAME) \
	-DMOZ_APP_DISPLAYNAME='$(MOZ_APP_DISPLAYNAME)' \
	$(RPM_INCIDENTALS)/mozilla.desktop \
	-o $(RPMBUILD_SOURCEDIR)/$(MOZ_APP_NAME).desktop && \
  rm -rf $(_ABS_DIST)/$(TARGET_CPU) && \
  $(RPMBUILD) -bb \
  $(SPEC_FILE) \
  --target $(TARGET_CPU) \
  --buildroot $(RPMBUILD_TOPDIR)/BUILDROOT \
  --define 'moz_app_name $(MOZ_APP_NAME)' \
  --define 'moz_app_displayname $(MOZ_APP_DISPLAYNAME)' \
  --define 'moz_app_version $(MOZ_APP_VERSION)' \
  --define 'moz_numeric_app_version $(MOZ_NUMERIC_APP_VERSION)' \
  --define 'moz_rpm_release $(MOZ_RPM_RELEASE)' \
  --define 'buildid $(BUILDID)' \
  $(if $(MOZ_SOURCE_REPO),--define 'moz_source_repo $(MOZ_SOURCE_REPO)') \
  --define 'moz_source_stamp $(MOZ_SOURCE_STAMP)' \
  --define 'moz_branding_directory $(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)' \
  --define '_topdir $(RPMBUILD_TOPDIR)' \
  --define '_rpmdir $(RPMBUILD_RPMDIR)' \
  --define '_sourcedir $(RPMBUILD_SOURCEDIR)' \
  --define '_specdir $(RPMBUILD_SPECDIR)' \
  --define '_srcrpmdir $(RPMBUILD_SRPMDIR)' \
  --define '_builddir $(RPMBUILD_BUILDDIR)' \
  --define '_prefix $(prefix)' \
  --define '_libdir $(libdir)' \
  --define '_bindir $(bindir)' \
  --define '_datadir $(datadir)' \
  --define '_installdir $(installdir)'

ifdef ENABLE_TESTS
RPM_CMD += \
  --define 'createtests yes' \
  --define '_testsinstalldir $(shell basename $(installdir))'
endif

ifdef INSTALL_SDK
RPM_CMD += \
  --define 'createdevel yes' \
  --define '_idldir $(idldir)' \
  --define '_sdkdir $(sdkdir)' \
  --define '_includedir $(includedir)'
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

JAVA_CLASSPATH = $(ANDROID_SDK)/android.jar
include $(MOZILLA_DIR)/config/android-common.mk

DIST_FILES =

# Place the files in the order they are going to be opened by the linker
ifndef MOZ_FOLD_LIBS
DIST_FILES += \
  libnspr4.so \
  libplc4.so \
  libplds4.so \
  libmozsqlite3.so \
  libnssutil3.so \
  $(NULL)
endif
DIST_FILES += libnss3.so
ifndef MOZ_FOLD_LIBS
DIST_FILES += \
  libssl3.so \
  libsmime3.so \
  $(NULL)
endif
DIST_FILES += \
  libxul.so \
  libnssckbi.so \
  libfreebl3.so \
  libsoftokn3.so \
  resources.arsc \
  AndroidManifest.xml \
  chrome \
  components \
  defaults \
  modules \
  hyphenation \
  res \
  lib \
  extensions \
  application.ini \
  package-name.txt \
  platform.ini \
  greprefs.js \
  browserconfig.properties \
  blocklist.xml \
  chrome.manifest \
  update.locale \
  removed-files \
  distribution \
  $(NULL)

NON_DIST_FILES = \
  classes.dex \
  $(NULL)

UPLOAD_EXTRA_FILES += gecko-unsigned-unaligned.apk

DIST_FILES += $(MOZ_CHILD_PROCESS_NAME)

GECKO_APP_AP_PATH = $(abspath $(DEPTH)/mobile/android/base)

ifdef ENABLE_TESTS
INNER_ROBOCOP_PACKAGE=echo
ifeq ($(MOZ_BUILD_APP),mobile/android)
UPLOAD_EXTRA_FILES += robocop.apk
UPLOAD_EXTRA_FILES += fennec_ids.txt
UPLOAD_EXTRA_FILES += geckoview_library/geckoview_library.zip
UPLOAD_EXTRA_FILES += geckoview_library/geckoview_assets.zip
UPLOAD_EXTRA_FILES += ../embedding/android/geckoview_example/geckoview_example.apk

# Robocop/Robotium tests, Android Background tests, and Fennec need to
# be signed with the same key, which means release signing them all.

ROBOCOP_PATH = $(abspath $(_ABS_DIST)/../build/mobile/robocop)
# Normally, $(NSINSTALL) would be used instead of cp, but INNER_ROBOCOP_PACKAGE
# is used in a series of commands that run under a "cd something", while
# $(NSINSTALL) is relative.
INNER_ROBOCOP_PACKAGE= \
  cp $(GECKO_APP_AP_PATH)/fennec_ids.txt $(_ABS_DIST) && \
  $(call RELEASE_SIGN_ANDROID_APK,$(ROBOCOP_PATH)/robocop-debug-unsigned-unaligned.apk,$(_ABS_DIST)/robocop.apk)
endif
else
INNER_ROBOCOP_PACKAGE=echo 'Testing is disabled - No Android Robocop for you'
endif

# Create geckoview_library/geckoview_{assets,library}.zip for third-party GeckoView consumers.
ifdef NIGHTLY_BUILD
ifndef MOZ_DISABLE_GECKOVIEW
INNER_MAKE_GECKOVIEW_LIBRARY= \
  $(MAKE) -C ../mobile/android/geckoview_library package
INNER_MAKE_GECKOVIEW_EXAMPLE= \
	$(MAKE) -C ../embedding/android/geckoview_example package
else
INNER_MAKE_GECKOVIEW_LIBRARY=echo 'GeckoView library packaging is disabled'
INNER_MAKE_GECKOVIEW_EXAMPLE=echo 'GeckoView example packaging is disabled'
endif
else
INNER_MAKE_GECKOVIEW_LIBRARY=echo 'GeckoView library packaging is only enabled on Nightly'
INNER_MAKE_GECKOVIEW_EXAMPLE=echo 'GeckoView example packaging is only enabled on Nightly'
endif

# Create Android ARchives and metadata for download by local
# developers using Gradle.
ifdef MOZ_ANDROID_GECKOLIBS_AAR
geckoaar-revision := $(BUILDID)

UPLOAD_EXTRA_FILES += \
  geckolibs-$(geckoaar-revision).aar \
  geckolibs-$(geckoaar-revision).aar.sha1 \
  geckolibs-$(geckoaar-revision).pom \
  geckolibs-$(geckoaar-revision).pom.sha1 \
  ivy-geckolibs-$(geckoaar-revision).xml \
  ivy-geckolibs-$(geckoaar-revision).xml.sha1 \
  geckoview-$(geckoaar-revision).aar \
  geckoview-$(geckoaar-revision).aar.sha1 \
  geckoview-$(geckoaar-revision).pom \
  geckoview-$(geckoaar-revision).pom.sha1 \
  ivy-geckoview-$(geckoaar-revision).xml \
  ivy-geckoview-$(geckoaar-revision).xml.sha1 \
  $(NULL)

INNER_MAKE_GECKOLIBS_AAR= \
  $(PYTHON) -m mozbuild.action.package_geckolibs_aar \
    --verbose \
    --revision $(geckoaar-revision) \
    --topsrcdir '$(topsrcdir)' \
    --distdir '$(_ABS_DIST)' \
    '$(_ABS_DIST)'
else
INNER_MAKE_GECKOLIBS_AAR=echo 'Android geckolibs.aar packaging is disabled'
endif

ifdef MOZ_OMX_PLUGIN
DIST_FILES += libomxplugin.so libomxplugingb.so libomxplugingb235.so \
              libomxpluginhc.so libomxpluginkk.so
endif

SO_LIBRARIES := $(filter %.so,$(DIST_FILES))
# These libraries are placed in the assets/$(ANDROID_CPU_ARCH) directory by packager.py.
ASSET_SO_LIBRARIES := $(addprefix assets/$(ANDROID_CPU_ARCH)/,$(filter-out libmozglue.so $(MOZ_CHILD_PROCESS_NAME),$(SO_LIBRARIES)))

DIST_FILES := $(filter-out $(SO_LIBRARIES),$(DIST_FILES))
NON_DIST_FILES += libmozglue.so $(MOZ_CHILD_PROCESS_NAME) $(ASSET_SO_LIBRARIES)

ifdef MOZ_ENABLE_SZIP
# These libraries are szipped in-place in the
# assets/$(ANDROID_CPU_ARCH) directory.
SZIP_LIBRARIES := $(ASSET_SO_LIBRARIES)
endif

# Fennec's OMNIJAR_NAME can include a directory; for example, it might
# be "assets/omni.ja". This path specifies where the omni.ja file
# lives in the APK, but should not root the resources it contains
# under assets/ (i.e., resources should not live at chrome://assets/).
# packager.py writes /omni.ja in order to be consistent with the
# layout expected by language repacks. Therefore, we move it to the
# correct path here, in INNER_MAKE_PACKAGE. See comment about
# OMNIJAR_NAME in configure.in.

# OMNIJAR_DIR is './' for "omni.ja", 'assets/' for "assets/omni.ja".
OMNIJAR_DIR := $(dir $(OMNIJAR_NAME))
OMNIJAR_NAME := $(notdir $(OMNIJAR_NAME))

# We force build an ap_ that does not check dependencies below.
# Language repacks take advantage of this unchecked dependency ap_ to
# insert additional resources (translated strings) into the ap_
# without the build system's participation.  This can do the wrong
# thing if there are resource changes in between build time and
# package time.  We try to prevent mismatched resources by erroring
# out if the compiled resource IDs are not the same as the resource
# IDs being packaged.  If we're doing a single locale repack, however,
# we don't have a complete object directory, so we can't compare
# resource IDs.

# A note on the res/ directory.  We unzip the ap_ during packaging,
# which produces the res/ directory.  This directory is then included
# in the final package.  When we unpack (during locale repacks), we
# need to remove the res/ directory because these resources confuse
# the l10n packaging script that updates omni.ja: the script tries to
# localize the contents of the res/ directory, which fails.  Instead,
# after the l10n packaging script completes, we build the ap_
# described above (which includes freshly localized Android resources)
# and the res/ directory is taken from the ap_ as part of the regular
# packaging.

PKG_SUFFIX      = .apk
INNER_MAKE_PACKAGE	= \
  $(if $(ALREADY_SZIPPED),,$(foreach lib,$(SZIP_LIBRARIES),host/bin/szip $(MOZ_SZIP_FLAGS) $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(lib) && )) \
  make -C $(GECKO_APP_AP_PATH) gecko-nodeps.ap_ && \
  cp $(GECKO_APP_AP_PATH)/gecko-nodeps.ap_ $(_ABS_DIST)/gecko.ap_ && \
  ( (test ! -f $(GECKO_APP_AP_PATH)/R.txt && echo "*** Warning: The R.txt that is being packaged might not agree with the R.txt that was built. This is normal during l10n repacks.") || \
    diff $(GECKO_APP_AP_PATH)/R.txt $(GECKO_APP_AP_PATH)/gecko-nodeps/R.txt >/dev/null || \
    (echo "*** Error: The R.txt that was built and the R.txt that is being packaged are not the same. Rebuild mobile/android/base and re-package." && exit 1)) && \
  ( cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && \
    unzip -o $(_ABS_DIST)/gecko.ap_ && \
    rm $(_ABS_DIST)/gecko.ap_ && \
    $(ZIP) $(if $(MOZ_ENABLE_SZIP),-0 )$(_ABS_DIST)/gecko.ap_ $(ASSET_SO_LIBRARIES) && \
    $(ZIP) -r9D $(_ABS_DIST)/gecko.ap_ $(DIST_FILES) -x $(NON_DIST_FILES) $(SZIP_LIBRARIES) && \
    $(if $(filter-out ./,$(OMNIJAR_DIR)), \
      mkdir -p $(OMNIJAR_DIR) && mv $(OMNIJAR_NAME) $(OMNIJAR_DIR) && ) \
    $(ZIP) -0 $(_ABS_DIST)/gecko.ap_ $(OMNIJAR_DIR)$(OMNIJAR_NAME)) && \
  rm -f $(_ABS_DIST)/gecko.apk && \
  cp $(_ABS_DIST)/gecko.ap_ $(_ABS_DIST)/gecko.apk && \
  $(ZIP) -j0 $(_ABS_DIST)/gecko.apk $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/classes.dex && \
  cp $(_ABS_DIST)/gecko.apk $(_ABS_DIST)/gecko-unsigned-unaligned.apk && \
  $(RELEASE_JARSIGNER) $(_ABS_DIST)/gecko.apk && \
  $(ZIPALIGN) -f -v 4 $(_ABS_DIST)/gecko.apk $(PACKAGE) && \
  $(INNER_ROBOCOP_PACKAGE) && \
  $(INNER_MAKE_GECKOLIBS_AAR) && \
  $(INNER_MAKE_GECKOVIEW_LIBRARY) && \
  $(INNER_MAKE_GECKOVIEW_EXAMPLE)

# Language repacks root the resources contained in assets/omni.ja
# under assets/, but the repacks expect them to be rooted at /.
# Therefore, we we move the omnijar back to / so the resources are
# under the root here, in INNER_UNMAKE_PACKAGE. See comments about
# OMNIJAR_NAME earlier in this file and in configure.in.

INNER_UNMAKE_PACKAGE	= \
  mkdir $(MOZ_PKG_DIR) && \
  ( cd $(MOZ_PKG_DIR) && \
    $(UNZIP) $(UNPACKAGE) && \
    rm -rf res \
    $(if $(filter-out ./,$(OMNIJAR_DIR)), \
      && mv $(OMNIJAR_DIR)$(OMNIJAR_NAME) $(OMNIJAR_NAME)) )
endif

ifeq ($(MOZ_PKG_FORMAT),DMG)
PKG_SUFFIX	= .dmg
PKG_DMG_FLAGS	=
ifneq (,$(MOZ_PKG_MAC_DSSTORE))
PKG_DMG_FLAGS += --copy '$(MOZ_PKG_MAC_DSSTORE):/.DS_Store'
endif
ifneq (,$(MOZ_PKG_MAC_BACKGROUND))
PKG_DMG_FLAGS += --mkdir /.background --copy '$(MOZ_PKG_MAC_BACKGROUND):/.background'
endif
ifneq (,$(MOZ_PKG_MAC_ICON))
PKG_DMG_FLAGS += --icon '$(MOZ_PKG_MAC_ICON)'
endif
ifneq (,$(MOZ_PKG_MAC_RSRC))
PKG_DMG_FLAGS += --resource '$(MOZ_PKG_MAC_RSRC)'
endif
ifneq (,$(MOZ_PKG_MAC_EXTRA))
PKG_DMG_FLAGS += $(MOZ_PKG_MAC_EXTRA)
endif
_ABS_MOZSRCDIR = $(shell cd $(MOZILLA_DIR) && pwd)
ifndef PKG_DMG_SOURCE
PKG_DMG_SOURCE = $(STAGEPATH)$(MOZ_PKG_DIR)
endif
INNER_MAKE_PACKAGE	= $(_ABS_MOZSRCDIR)/build/package/mac_osx/pkg-dmg \
  --source '$(PKG_DMG_SOURCE)' --target '$(PACKAGE)' \
  --volname '$(MOZ_APP_DISPLAYNAME)' $(PKG_DMG_FLAGS)
INNER_UNMAKE_PACKAGE	= \
  set -ex; \
  rm -rf $(_ABS_DIST)/unpack.tmp; \
  mkdir -p $(_ABS_DIST)/unpack.tmp; \
  $(_ABS_MOZSRCDIR)/build/package/mac_osx/unpack-diskimage $(UNPACKAGE) /tmp/$(MOZ_PKG_APPNAME)-unpack $(_ABS_DIST)/unpack.tmp; \
  rsync -a '$(_ABS_DIST)/unpack.tmp/$(_APPNAME)' $(MOZ_PKG_DIR); \
  test -n '$(MOZ_PKG_MAC_DSSTORE)' && \
    rsync -a '$(_ABS_DIST)/unpack.tmp/.DS_Store' '$(MOZ_PKG_MAC_DSSTORE)'; \
  test -n '$(MOZ_PKG_MAC_BACKGROUND)' && \
    rsync -a '$(_ABS_DIST)/unpack.tmp/.background/$(notdir $(MOZ_PKG_MAC_BACKGROUND))' '$(MOZ_PKG_MAC_BACKGROUND)'; \
  test -n '$(MOZ_PKG_MAC_ICON)' && \
    rsync -a '$(_ABS_DIST)/unpack.tmp/.VolumeIcon.icns' '$(MOZ_PKG_MAC_ICON)'; \
  rm -rf $(_ABS_DIST)/unpack.tmp; \
  if test -n '$(MOZ_PKG_MAC_RSRC)' ; then \
    cp $(UNPACKAGE) $(MOZ_PKG_APPNAME).tmp.dmg && \
    hdiutil unflatten $(MOZ_PKG_APPNAME).tmp.dmg && \
    { /Developer/Tools/DeRez -skip plst -skip blkx $(MOZ_PKG_APPNAME).tmp.dmg > '$(MOZ_PKG_MAC_RSRC)' || { rm -f $(MOZ_PKG_APPNAME).tmp.dmg && false; }; } && \
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

ifdef MOZ_INTERNAL_SIGNING_FORMAT
MOZ_SIGN_PREPARED_PACKAGE_CMD=$(MOZ_SIGN_CMD) $(foreach f,$(MOZ_INTERNAL_SIGNING_FORMAT),-f $(f)) $(foreach i,$(SIGN_INCLUDES),-i $(i)) $(foreach x,$(SIGN_EXCLUDES),-x $(x))
ifeq (WINNT,$(OS_ARCH))
MOZ_SIGN_PREPARED_PACKAGE_CMD += --nsscmd '$(_ABS_DIST)/bin/shlibsign$(BIN_SUFFIX) -v -i'
endif
endif

# For final GPG / authenticode signing / dmg signing if required
ifdef MOZ_EXTERNAL_SIGNING_FORMAT
MOZ_SIGN_PACKAGE_CMD=$(MOZ_SIGN_CMD) $(foreach f,$(MOZ_EXTERNAL_SIGNING_FORMAT),-f $(f))
endif

ifdef MOZ_SIGN_PREPARED_PACKAGE_CMD
ifeq (Darwin, $(OS_ARCH))
MAKE_PACKAGE    = (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_RESPATH) && $(CREATE_PRECOMPLETE_CMD)) \
                  && cd ./$(PKG_DMG_SOURCE) && $(MOZ_SIGN_PREPARED_PACKAGE_CMD) $(MOZ_MACBUNDLE_NAME) \
                  && cd $(PACKAGE_BASE_DIR) && $(INNER_MAKE_PACKAGE)
else
MAKE_PACKAGE    = $(MOZ_SIGN_PREPARED_PACKAGE_CMD) $(MOZ_PKG_DIR) \
                  && $(or $(call MAKE_SIGN_EME_VOUCHER,$(STAGEPATH)$(MOZ_PKG_DIR)),true) \
                  && (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_RESPATH) && $(CREATE_PRECOMPLETE_CMD)) \
                  && $(INNER_MAKE_PACKAGE)
endif #Darwin

else
MAKE_PACKAGE    = (cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_RESPATH) && $(CREATE_PRECOMPLETE_CMD)) && $(INNER_MAKE_PACKAGE)
endif

ifdef MOZ_SIGN_PACKAGE_CMD
MAKE_PACKAGE    += && $(MOZ_SIGN_PACKAGE_CMD) '$(PACKAGE)'
endif

ifdef MOZ_SIGN_CMD
MAKE_SDK           += && $(MOZ_SIGN_CMD) -f gpg $(SDK)
UPLOAD_EXTRA_FILES += $(SDK).asc
endif

NO_PKG_FILES += \
	core \
	bsdecho \
	js \
	js-config \
	jscpucfg \
	nsinstall \
	viewer \
	TestGtkEmbed \
	elf-dynstr-gc \
	mangle* \
	maptsv* \
	mfc* \
	msdump* \
	msmap* \
	nm2tsv* \
	nsinstall* \
	res/samples \
	res/throbber \
	shlibsign* \
	certutil* \
	pk12util* \
	BadCertServer* \
	ClientAuthServer* \
	OCSPStaplingServer* \
	GenerateOCSPResponse* \
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

# If a manifest has not been supplied, the following
# files should be excluded from the package too
ifndef MOZ_PKG_MANIFEST
NO_PKG_FILES += ssltunnel*
endif

ifdef MOZ_DMD
NO_PKG_FILES += SmokeDMD
endif

DEFINES += -DDLL_PREFIX=$(DLL_PREFIX) -DDLL_SUFFIX=$(DLL_SUFFIX) -DBIN_SUFFIX=$(BIN_SUFFIX)

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
DEFINES += -DDIR_MACOS=Contents/MacOS/ -DDIR_RESOURCES=Contents/Resources/
else
DEFINES += -DDIR_MACOS= -DDIR_RESOURCES=
endif

ifdef MOZ_FOLD_LIBS
DEFINES += -DMOZ_FOLD_LIBS=1
endif

GARBAGE		+= $(DIST)/$(PACKAGE) $(PACKAGE)

# The following target stages files into two directories: one directory for
# core files, and one for optional extensions based on the information in
# the MOZ_PKG_MANIFEST file.

PKG_ARG = , '$(pkg)'

# MOZ_PKG_MANIFEST is the canonical way to define the package manifest (which
# the packager will preprocess), but for a smooth transition, we derive it
# from the now deprecated MOZ_PKG_MANIFEST_P when MOZ_PKG_MANIFEST is not
# defined.
ifndef MOZ_PKG_MANIFEST
ifdef MOZ_PKG_MANIFEST_P
MOZ_PKG_MANIFEST := $(MOZ_PKG_MANIFEST_P)
endif # MOZ_PKG_MANIFEST_P
endif # MOZ_PKG_MANIFEST

# For smooth transition of comm-central
ifndef MOZ_PACKAGER_FORMAT
ifeq ($(MOZ_CHROME_FILE_FORMAT),flat)
ifdef MOZ_OMNIJAR
MOZ_PACKAGER_FORMAT := omni
else
MOZ_PACKAGER_FORMAT := flat
endif
endif
endif
ifndef MOZ_PACKAGER_FORMAT
MOZ_PACKAGER_FORMAT = $(error MOZ_PACKAGER_FORMAT is not set)
endif

ifneq (android,$(MOZ_WIDGET_TOOLKIT))
OPTIMIZEJARS = 1
endif

# A js binary is needed to perform verification of JavaScript minification.
# We can only use the built binary when not cross-compiling. Environments
# (such as release automation) can provide their own js binary to enable
# verification when cross-compiling.
ifndef JS_BINARY
ifndef CROSS_COMPILE
JS_BINARY = $(wildcard $(DIST)/bin/js)
endif
endif

ifeq ($(OS_TARGET), WINNT)
INSTALLER_PACKAGE = $(DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe
endif

# These are necessary because some of our packages/installers contain spaces
# in their filenames and GNU Make's $(wildcard) function doesn't properly
# deal with them.
empty :=
space = $(empty) $(empty)
QUOTED_WILDCARD = $(if $(wildcard $(subst $(space),?,$(1))),'$(1)')
ESCAPE_SPACE = $(subst $(space),\$(space),$(1))
ESCAPE_WILDCARD = $(subst $(space),?,$(1))

# This variable defines which OpenSSL algorithm to use to
# generate checksums for files that we upload
CHECKSUM_ALGORITHM_PARAM = -d sha512 -d md5 -d sha1

# This variable defines where the checksum file will be located
CHECKSUM_FILE = '$(DIST)/$(PKG_PATH)/$(CHECKSUMS_FILE_BASENAME).checksums'
CHECKSUM_FILES = $(CHECKSUM_FILE)

ifeq (WINNT,$(OS_TARGET))
UPLOAD_EXTRA_FILES += host/bin/mar.exe
UPLOAD_EXTRA_FILES += host/bin/mbsdiff.exe
else
UPLOAD_EXTRA_FILES += host/bin/mar
UPLOAD_EXTRA_FILES += host/bin/mbsdiff
endif

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
  $(call QUOTED_WILDCARD,$(MOZ_BUILDINFO_FILE)) \
  $(call QUOTED_WILDCARD,$(MOZ_MOZINFO_FILE)) \
  $(call QUOTED_WILDCARD,$(PKG_JSSHELL)) \
  $(if $(UPLOAD_EXTRA_FILES), $(foreach f, $(UPLOAD_EXTRA_FILES), $(wildcard $(DIST)/$(f))))

ifdef MOZ_CRASHREPORTER_UPLOAD_FULL_SYMBOLS
UPLOAD_FILES += \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(SYMBOL_FULL_ARCHIVE_BASENAME).zip)
endif

ifdef MOZ_CODE_COVERAGE
UPLOAD_FILES += \
  $(call QUOTED_WILDCARD,$(DIST)/$(PKG_PATH)$(CODE_COVERAGE_ARCHIVE_BASENAME).zip)
endif

SIGN_CHECKSUM_CMD=
ifdef MOZ_SIGN_CMD
# If we're signing with gpg, we'll have a bunch of extra detached signatures to
# upload. We also want to sign our checksums file
SIGN_CHECKSUM_CMD=$(MOZ_SIGN_CMD) -f gpg $(CHECKSUM_FILE)

CHECKSUM_FILES += $(CHECKSUM_FILE).asc
UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(COMPLETE_MAR).asc)
UPLOAD_FILES += $(call QUOTED_WILDCARD,$(wildcard $(DIST)/$(PARTIAL_MAR).asc))
UPLOAD_FILES += $(call QUOTED_WILDCARD,$(INSTALLER_PACKAGE).asc)
UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PACKAGE).asc)
endif

ifdef MOZ_STUB_INSTALLER
UPLOAD_FILES += $(call QUOTED_WILDCARD,$(DIST)/$(PKG_INST_PATH)$(PKG_STUB_BASENAME).exe)
endif

ifndef MOZ_PKG_SRCDIR
MOZ_PKG_SRCDIR = $(topsrcdir)
endif

DIR_TO_BE_PACKAGED ?= ../$(notdir $(topsrcdir))
SRC_TAR_EXCLUDE_PATHS += \
  --exclude='.hg*' \
  --exclude='CVS' \
  --exclude='.cvs*' \
  --exclude='.mozconfig*' \
  --exclude='*.pyc' \
  --exclude='$(MOZILLA_DIR)/Makefile' \
  --exclude='$(MOZILLA_DIR)/dist'
ifdef MOZ_OBJDIR
SRC_TAR_EXCLUDE_PATHS += --exclude='$(MOZ_OBJDIR)'
endif
CREATE_SOURCE_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode=go-w $(SRC_TAR_EXCLUDE_PATHS) -f

SOURCE_TAR = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_SRCPACK_BASENAME).tar.bz2
HG_BUNDLE_FILE = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_BUNDLE_BASENAME).bundle
SOURCE_CHECKSUM_FILE = $(DIST)/$(PKG_SRCPACK_PATH)$(PKG_SRCPACK_BASENAME).checksums
SOURCE_UPLOAD_FILES = $(SOURCE_TAR)

HG ?= hg
CREATE_HG_BUNDLE_CMD  = $(HG) -v -R $(topsrcdir) bundle --base null
ifdef HG_BUNDLE_REVISION
CREATE_HG_BUNDLE_CMD += -r $(HG_BUNDLE_REVISION)
endif
CREATE_HG_BUNDLE_CMD += $(HG_BUNDLE_FILE)
ifdef UPLOAD_HG_BUNDLE
SOURCE_UPLOAD_FILES  += $(HG_BUNDLE_FILE)
endif
