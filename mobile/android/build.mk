# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include  $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

installer:
	@$(MAKE) -C mobile/android/installer installer

package:
	@$(MAKE) -C mobile/android/installer

stage-package:
	$(MAKE) -C mobile/android/installer stage-package

deb: package
	@$(MAKE) -C mobile/android/installer deb

upload::
	@$(MAKE) -C mobile/android/installer upload

wget-en-US:
	@$(MAKE) -C mobile/android/locales $@

# make -j1 because dependencies in l10n build targets don't work
# with parallel builds
merge-% chrome-%:
	$(MAKE) -j1 -C mobile/android/locales $@

ifdef ENABLE_TESTS
# Implemented in testing/testsuite-targets.mk

mochitest-browser-chrome:
	$(RUN_MOCHITEST) --flavor=browser
	$(CHECK_TEST_ERROR)

mochitest:: mochitest-browser-chrome

.PHONY: mochitest-browser-chrome
endif

ifeq ($(OS_TARGET),Linux)
deb: installer
endif
