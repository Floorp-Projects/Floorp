# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file should ONLY be included from upload-files.mk. It was
# split into its own file to increase comprehension of
# upload-files.mk.

# DEBUG_JARSIGNER always debug signs.
DEBUG_JARSIGNER=$(PYTHON) $(abspath $(topsrcdir)/mobile/android/debug_sign_tool.py) \
  --keytool=$(KEYTOOL) \
  --jarsigner=$(JARSIGNER) \
  $(NULL)

RELEASE_JARSIGNER := $(DEBUG_JARSIGNER)

# $(1) is the full path to input:  foo-debug-unsigned-unaligned.apk.
# $(2) is the full path to output: foo.apk.
# Use this like: $(call RELEASE_SIGN_ANDROID_APK,foo-debug-unsigned-unaligned.apk,foo.apk)
#
# The |zip -d| there to handle re-signing previously signed APKs.  Gradle
# produces signed, unaligned APK files, but this expects unsigned, unaligned
# APK files.  The |zip -d| discards any existing signature, turning a signed,
# unaligned APK into an unsigned, unaligned APK.  Sadly |zip -q| doesn't
# silence a warning about "nothing to do" so we pipe to /dev/null.
RELEASE_SIGN_ANDROID_APK = \
  cp $(1) $(2)-unaligned.apk && \
  ($(ZIP) -d $(2)-unaligned.apk 'META-INF/*' > /dev/null || true) && \
  $(RELEASE_JARSIGNER) $(2)-unaligned.apk && \
  $(ZIPALIGN) $(if $(MOZ_AUTOMATION),-v) -f 4 $(2)-unaligned.apk $(2) && \
  $(RM) $(2)-unaligned.apk

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

ifdef ENABLE_TESTS
INNER_ROBOCOP_PACKAGE=true
ifeq ($(MOZ_BUILD_APP),mobile/android)
UPLOAD_EXTRA_FILES += robocop.apk

# Robocop/Robotium tests and Fennec need to be signed with the same
# key, which means release signing them all.

INNER_ROBOCOP_PACKAGE= \
  $(call RELEASE_SIGN_ANDROID_APK,$(GRADLE_ANDROID_APP_ANDROIDTEST_APK),$(ABS_DIST)/robocop.apk)
endif
else
INNER_ROBOCOP_PACKAGE=echo 'Testing is disabled - No Android Robocop for you'
endif


# We force build an ap_ that does not check dependencies below.
# Language repacks take advantage of this unchecked dependency ap_ to
# insert additional resources (translated strings) into the ap_
# without the build system's participation.  This can do the wrong
# thing if there are resource changes in between build time and
# package time.
PKG_SUFFIX = .apk

INNER_FENNEC_PACKAGE = \
  $(PYTHON) -m mozbuild.action.package_fennec_apk \
    --verbose \
    --inputs $(GRADLE_ANDROID_APP_APK) \
    --omnijar $(MOZ_PKG_DIR)/$(OMNIJAR_NAME) \
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
  $(PYTHON) -m mozbuild.action.package_fennec_apk \
    --verbose \
    --inputs \
      $(UNPACKAGE) \
      $(GRADLE_ANDROID_APP_APK) \
    --omnijar $(MOZ_PKG_DIR)/$(OMNIJAR_NAME) \
    --output $(PACKAGE:.apk=-unsigned-unaligned.apk) && \
  $(call RELEASE_SIGN_ANDROID_APK,$(PACKAGE:.apk=-unsigned-unaligned.apk),$(PACKAGE))

INNER_MAKE_PACKAGE = $(if $(UNPACKAGE),$(repackage_fennec),$(package_fennec))

INNER_UNMAKE_PACKAGE = \
  mkdir $(MOZ_PKG_DIR) && \
  ( cd $(MOZ_PKG_DIR) && \
    $(UNZIP) $(UNPACKAGE) $(ROOT_FILES) $(OMNIJAR_NAME) )
