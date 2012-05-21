# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef LIBXUL_SDK
# Needed for building our components as part of libxul
APP_LIBXUL_DIRS += mobile/xul/components/build

include $(topsrcdir)/toolkit/toolkit-tiers.mk
else
ifdef ENABLE_TESTS
tier_testharness_dirs += \
  testing/mochitest \
  $(NULL)
endif
endif

TIERS += app

ifdef MOZ_EXTENSIONS
tier_app_dirs += extensions
endif

ifdef MOZ_SERVICES_SYNC
tier_app_dirs += services
endif

tier_app_dirs += \
  $(MOZ_BRANDING_DIRECTORY) \
  mobile/xul \
  $(NULL)


installer: 
	@$(MAKE) -C mobile/xul/installer installer

package:
	@$(MAKE) -C mobile/xul/installer

fast-package:
	@$(MAKE) package MOZ_FAST_PACKAGE=1

install::
	@echo "Mobile can't be installed directly."
	@exit 1

deb: package
	@$(MAKE) -C mobile/xul/installer deb

upload::
	@$(MAKE) -C mobile/xul/installer upload

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
