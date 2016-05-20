# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include  $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

installer:
	@$(MAKE) -C mobile/android/installer installer

package:
	@$(MAKE) -C mobile/android/installer

ifeq ($(OS_TARGET),Android)
ifneq ($(MOZ_ANDROID_INSTALL_TARGET),)
ANDROID_SERIAL = $(MOZ_ANDROID_INSTALL_TARGET)
endif
ifneq ($(ANDROID_SERIAL),)
export ANDROID_SERIAL
else
# Determine if there's more than one device connected
android_devices=$(words $(filter device,$(shell $(ADB) devices)))
define no_device
	@echo 'No devices are connected.  Connect a device or start an emulator.'
	@exit 1
endef
define multiple_devices
	@echo 'Multiple devices are connected. Define ANDROID_SERIAL to specify the install target.'
	$(ADB) devices
	@exit 1
endef

install::
	@# Use foreach to avoid running adb multiple times here
	$(foreach val,$(android_devices),\
		$(if $(filter 0,$(val)),$(no_device),\
			$(if $(filter-out 1,$(val)),$(multiple_devices))))
endif

install::
	$(ADB) install -r $(DIST)/$(PKG_PATH)$(PKG_BASENAME).apk
else
	@echo 'Mobile can't be installed directly.'
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
