# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file should ONLY be included from upload-files.mk. It was
# split into its own file to increase comprehension of
# upload-files.mk.

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
  liblgpllibs.so \
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
  ua-update.json \
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

DIST_FILES += $(MOZ_CHILD_PROCESS_NAME)

GECKO_APP_AP_PATH = $(topobjdir)/mobile/android/base

ifdef ENABLE_TESTS
INNER_ROBOCOP_PACKAGE=true
ifeq ($(MOZ_BUILD_APP),mobile/android)
UPLOAD_EXTRA_FILES += robocop.apk
UPLOAD_EXTRA_FILES += fennec_ids.txt
UPLOAD_EXTRA_FILES += geckoview_library/geckoview_library.zip
UPLOAD_EXTRA_FILES += geckoview_library/geckoview_assets.zip

# Robocop/Robotium tests, Android Background tests, and Fennec need to
# be signed with the same key, which means release signing them all.

ifndef MOZ_BUILD_MOBILE_ANDROID_WITH_GRADLE
robocop_apk := $(topobjdir)/mobile/android/tests/browser/robocop/robocop-debug-unsigned-unaligned.apk
else
robocop_apk := $(topobjdir)/gradle/build/mobile/android/app/outputs/apk/app-automation-debug-androidTest-unaligned.apk
endif

# Normally, $(NSINSTALL) would be used instead of cp, but INNER_ROBOCOP_PACKAGE
# is used in a series of commands that run under a "cd something", while
# $(NSINSTALL) is relative.
INNER_ROBOCOP_PACKAGE= \
  cp $(GECKO_APP_AP_PATH)/fennec_ids.txt $(ABS_DIST) && \
  $(call RELEASE_SIGN_ANDROID_APK,$(robocop_apk),$(ABS_DIST)/robocop.apk)
endif
else
INNER_ROBOCOP_PACKAGE=echo 'Testing is disabled - No Android Robocop for you'
endif

ifdef MOZ_ANDROID_PACKAGE_INSTALL_BOUNCER
INNER_INSTALL_BOUNCER_PACKAGE=true
ifdef ENABLE_TESTS
UPLOAD_EXTRA_FILES += bouncer.apk

bouncer_package=$(ABS_DIST)/bouncer.apk

# Package and release sign the install bouncer APK.  This assumes that the main
# APK (that is, $(PACKAGE)) has already been produced, and verifies that the
# bouncer APK and the main APK define the same set of permissions.  The
# intention is to avoid permission-related surprises when bouncing to the
# installation process in the Play Store.  N.b.: sort -u is Posix and saves
# invoking uniq separately.  diff -u is *not* Posix, so we only add -c.
INNER_INSTALL_BOUNCER_PACKAGE=\
  $(call RELEASE_SIGN_ANDROID_APK,$(topobjdir)/mobile/android/bouncer/bouncer-unsigned-unaligned.apk,$(bouncer_package)) && \
  ($(AAPT) dump permissions $(PACKAGE) | sort -u > $(PACKAGE).permissions && \
   $(AAPT) dump permissions $(bouncer_package) | sort -u > $(bouncer_package).permissions && \
   diff -c $(PACKAGE).permissions $(bouncer_package).permissions || \
   (echo "*** Error: The permissions of the bouncer package differ from the permissions of the main package.  Ensure the bouncer and main package Android manifests agree, rebuild mobile/android, and re-package." && exit 1))
else
INNER_INSTALL_BOUNCER_PACKAGE=echo 'Testing is disabled, so the install bouncer is disabled - No trampolines for you'
endif # ENABLE_TESTS
else
INNER_INSTALL_BOUNCER_PACKAGE=echo 'Install bouncer is disabled - No trampolines for you'
endif # MOZ_ANDROID_PACKAGE_INSTALL_BOUNCER

# Create geckoview_library/geckoview_{assets,library}.zip for third-party GeckoView consumers.
ifdef NIGHTLY_BUILD
ifndef MOZ_DISABLE_GECKOVIEW
INNER_MAKE_GECKOVIEW_LIBRARY= \
  $(MAKE) -C ../mobile/android/geckoview_library package
else
INNER_MAKE_GECKOVIEW_LIBRARY=echo 'GeckoView library packaging is disabled'
endif
else
INNER_MAKE_GECKOVIEW_LIBRARY=echo 'GeckoView library packaging is only enabled on Nightly'
endif

# Create Android ARchives and metadata for download by local
# developers using Gradle.
ifdef MOZ_ANDROID_GECKOLIBS_AAR
ifndef MOZ_DISABLE_GECKOVIEW
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
    --distdir '$(ABS_DIST)' \
    --appname '$(MOZ_APP_NAME)' \
    --purge-old \
    '$(ABS_DIST)'
else
INNER_MAKE_GECKOLIBS_AAR=echo 'Android geckolibs.aar packaging requires packaging geckoview'
endif # MOZ_DISABLE_GECKOVIEW
else
INNER_MAKE_GECKOLIBS_AAR=echo 'Android geckolibs.aar packaging is disabled'
endif # MOZ_ANDROID_GECKOLIBS_AAR

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

ifndef COMPILE_ENVIRONMENT
# Any Fennec binary libraries we download are already szipped.
ALREADY_SZIPPED=1
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

PKG_SUFFIX = .apk

INNER_SZIP_LIBRARIES = \
  $(if $(ALREADY_SZIPPED),,$(foreach lib,$(SZIP_LIBRARIES),host/bin/szip $(MOZ_SZIP_FLAGS) $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/$(lib) && )) true

ifdef MOZ_BUILD_MOBILE_ANDROID_WITH_GRADLE
INNER_CHECK_R_TXT=echo 'No R.txt checking for you!'
else
INNER_CHECK_R_TXT=\
  ((test ! -f $(GECKO_APP_AP_PATH)/R.txt && echo "*** Warning: The R.txt that is being packaged might not agree with the R.txt that was built. This is normal during l10n repacks.") || \
    diff $(GECKO_APP_AP_PATH)/R.txt $(GECKO_APP_AP_PATH)/gecko-nodeps/R.txt >/dev/null || \
    (echo "*** Error: The R.txt that was built and the R.txt that is being packaged are not the same. Rebuild mobile/android/base and re-package." && exit 1))
endif

# Insert $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/classes.dex into
# $(ABS_DIST)/gecko.ap_, producing $(ABS_DIST)/gecko.apk.
INNER_MAKE_APK = \
  ( cd $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH) && \
    unzip -o $(ABS_DIST)/gecko.ap_ && \
    rm $(ABS_DIST)/gecko.ap_ && \
    $(ZIP) -r9D $(ABS_DIST)/gecko.ap_ assets && \
    $(ZIP) $(if $(ALREADY_SZIPPED),-0 ,$(if $(MOZ_ENABLE_SZIP),-0 ))$(ABS_DIST)/gecko.ap_ $(ASSET_SO_LIBRARIES) && \
    $(ZIP) -r9D $(ABS_DIST)/gecko.ap_ $(DIST_FILES) -x $(NON_DIST_FILES) $(SZIP_LIBRARIES) && \
    $(if $(filter-out ./,$(OMNIJAR_DIR)), \
      mkdir -p $(OMNIJAR_DIR) && mv $(OMNIJAR_NAME) $(OMNIJAR_DIR) && ) \
    $(ZIP) -0 $(ABS_DIST)/gecko.ap_ $(OMNIJAR_DIR)$(OMNIJAR_NAME)) && \
  rm -f $(ABS_DIST)/gecko.apk && \
  cp $(ABS_DIST)/gecko.ap_ $(ABS_DIST)/gecko.apk && \
  $(ZIP) -j0 $(ABS_DIST)/gecko.apk $(STAGEPATH)$(MOZ_PKG_DIR)$(_BINPATH)/classes.dex && \
  cp $(ABS_DIST)/gecko.apk $(ABS_DIST)/gecko-unsigned-unaligned.apk && \
  $(RELEASE_JARSIGNER) $(ABS_DIST)/gecko.apk && \
  $(ZIPALIGN) -f -v 4 $(ABS_DIST)/gecko.apk $(PACKAGE)

ifeq ($(MOZ_BUILD_APP),mobile/android)
INNER_MAKE_PACKAGE = \
  $(INNER_SZIP_LIBRARIES) && \
  make -C $(GECKO_APP_AP_PATH) gecko-nodeps.ap_ && \
  cp $(GECKO_APP_AP_PATH)/gecko-nodeps.ap_ $(ABS_DIST)/gecko.ap_ && \
  $(INNER_CHECK_R_TXT) && \
  $(INNER_MAKE_APK) && \
  $(INNER_ROBOCOP_PACKAGE) && \
  $(INNER_INSTALL_BOUNCER_PACKAGE) && \
  $(INNER_MAKE_GECKOLIBS_AAR) && \
  $(INNER_MAKE_GECKOVIEW_LIBRARY)
endif

ifeq ($(MOZ_BUILD_APP),mobile/android/b2gdroid)
INNER_MAKE_PACKAGE = \
  $(INNER_SZIP_LIBRARIES) && \
  cp $(topobjdir)/mobile/android/b2gdroid/app/classes.dex $(ABS_DIST)/classes.dex && \
  cp $(topobjdir)/mobile/android/b2gdroid/app/b2gdroid-unsigned-unaligned.apk $(ABS_DIST)/gecko.ap_ && \
  $(INNER_MAKE_APK)
endif

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
