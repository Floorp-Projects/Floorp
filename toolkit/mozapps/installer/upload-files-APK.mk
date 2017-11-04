# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file should ONLY be included from upload-files.mk. It was
# split into its own file to increase comprehension of
# upload-files.mk.

include $(MOZILLA_DIR)/config/android-common.mk

# Files packed into the APK root.  Packing files into the APK root is not
# supported by modern Android build systems, including Gradle, so don't add to
# this list without Android peer approval.
ROOT_FILES := \
  application.ini \
  package-name.txt \
  ua-update.json \
  platform.ini \
  removed-files \
  $(NULL)

GECKO_APP_AP_PATH = $(topobjdir)/mobile/android/base

ifdef ENABLE_TESTS
INNER_ROBOCOP_PACKAGE=true
ifeq ($(MOZ_BUILD_APP),mobile/android)
UPLOAD_EXTRA_FILES += robocop.apk

# Robocop/Robotium tests, Android Background tests, and Fennec need to
# be signed with the same key, which means release signing them all.

ifndef MOZ_BUILD_MOBILE_ANDROID_WITH_GRADLE
robocop_apk := $(topobjdir)/mobile/android/tests/browser/robocop/robocop-debug-unsigned-unaligned.apk
else
robocop_apk := $(topobjdir)/gradle/build/mobile/android/app/outputs/apk/app-official-photon-debug-androidTest.apk
endif

INNER_ROBOCOP_PACKAGE= \
  $(call RELEASE_SIGN_ANDROID_APK,$(robocop_apk),$(ABS_DIST)/robocop.apk)
endif
else
INNER_ROBOCOP_PACKAGE=echo 'Testing is disabled - No Android Robocop for you'
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
# package time.
PKG_SUFFIX = .apk

INNER_FENNEC_PACKAGE = \
  $(MAKE) -C $(GECKO_APP_AP_PATH) gecko-nodeps.ap_ && \
  $(PYTHON) -m mozbuild.action.package_fennec_apk \
    --verbose \
    --inputs \
      $(GECKO_APP_AP_PATH)/gecko-nodeps.ap_ \
    --omnijar $(MOZ_PKG_DIR)/$(OMNIJAR_NAME) \
    --classes-dex $(GECKO_APP_AP_PATH)/classes.dex \
    --lib-dirs $(MOZ_PKG_DIR)/lib \
    --assets-dirs $(MOZ_PKG_DIR)/assets \
    --features-dirs $(MOZ_PKG_DIR)/features \
    --root-files $(foreach f,$(ROOT_FILES),$(MOZ_PKG_DIR)/$(f)) \
    --output $(PACKAGE:.apk=-unsigned-unaligned.apk) && \
  $(call RELEASE_SIGN_ANDROID_APK,$(PACKAGE:.apk=-unsigned-unaligned.apk),$(PACKAGE))

# Packaging produces many optional artifacts.
package_fennec = \
  $(INNER_FENNEC_PACKAGE) && \
  $(INNER_ROBOCOP_PACKAGE)

# Re-packaging only replaces Android resources and the omnijar before
# (re-)signing.
repackage_fennec = \
  $(MAKE) -C $(GECKO_APP_AP_PATH) gecko-nodeps.ap_ && \
  $(PYTHON) -m mozbuild.action.package_fennec_apk \
    --verbose \
    --inputs \
      $(UNPACKAGE) \
      $(GECKO_APP_AP_PATH)/gecko-nodeps.ap_ \
    --omnijar $(MOZ_PKG_DIR)/$(OMNIJAR_NAME) \
    --classes-dex $(GECKO_APP_AP_PATH)/classes.dex \
    --output $(PACKAGE:.apk=-unsigned-unaligned.apk) && \
  $(call RELEASE_SIGN_ANDROID_APK,$(PACKAGE:.apk=-unsigned-unaligned.apk),$(PACKAGE))

INNER_MAKE_PACKAGE = $(if $(UNPACKAGE),$(repackage_fennec),$(package_fennec))

# Language repacks root the resources contained in assets/omni.ja
# under assets/, but the repacks expect them to be rooted at /.
# Therefore, we we move the omnijar back to / so the resources are
# under the root here, in INNER_UNMAKE_PACKAGE. See comments about
# OMNIJAR_NAME earlier in this file and in configure.in.

INNER_UNMAKE_PACKAGE = \
  mkdir $(MOZ_PKG_DIR) && \
  ( cd $(MOZ_PKG_DIR) && \
    $(UNZIP) $(UNPACKAGE) $(ROOT_FILES) && \
    $(UNZIP) $(UNPACKAGE) $(OMNIJAR_DIR)$(OMNIJAR_NAME) && \
    $(if $(filter-out ./,$(OMNIJAR_DIR)), \
      mv $(OMNIJAR_DIR)$(OMNIJAR_NAME) $(OMNIJAR_NAME), \
      true) )
