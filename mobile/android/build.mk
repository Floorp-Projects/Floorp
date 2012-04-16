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
# The Original Code is Mozilla.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation <http://www.mozilla.org/>.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Finkle <mfinkle@mozilla.com>
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

ifndef LIBXUL_SDK
# Needed for building our components as part of libxul
APP_LIBXUL_DIRS += mobile/android/components/build

include $(topsrcdir)/toolkit/toolkit-tiers.mk
else
ifdef ENABLE_TESTS
tier_testharness_dirs += \
  testing/mochitest \
  $(NULL)
endif
endif

include  $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

TIERS += app

ifdef MOZ_EXTENSIONS
tier_app_dirs += extensions
endif

ifdef MOZ_SERVICES_SYNC
tier_app_dirs += services
endif

tier_app_dirs += \
  $(MOZ_BRANDING_DIRECTORY) \
  mobile/android \
  $(NULL)


installer: 
	@$(MAKE) -C mobile/android/installer installer

package:
	@$(MAKE) -C mobile/android/installer

ifeq ($(OS_TARGET),Android)
ifeq ($(MOZ_ANDROID_INSTALL_TARGET),)
# Determine if there's more than one device connected
android_devices=$(filter device,$(shell $(ANDROID_PLATFORM_TOOLS)/adb devices))
ifneq ($(android_devices),device)
install::
	@echo "Multiple devices are connected. Define MOZ_ANDROID_INSTALL_TARGET to specify the install target."
	$(ANDROID_PLATFORM_TOOLS)/adb devices
	@exit 1
else
install::
	$(ANDROID_PLATFORM_TOOLS)/adb install -r $(DIST)/$(PKG_PATH)$(PKG_BASENAME).apk
endif
else
install::
	$(ANDROID_PLATFORM_TOOLS)/adb -s $(MOZ_ANDROID_INSTALL_TARGET) install -r $(DIST)/$(PKG_PATH)$(PKG_BASENAME).apk
endif
else
	@echo "Mobile can't be installed directly."
	@exit 1
endif

deb: package
	@$(MAKE) -C mobile/android/installer deb

upload::
	@$(MAKE) -C mobile/android/installer upload

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --browser-chrome
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome
endif

ifeq ($(OS_TARGET),Linux)
deb: installer
endif
