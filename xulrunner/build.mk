# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(topsrcdir)/toolkit/toolkit-tiers.mk

TIERS += app

ifdef MOZ_EXTENSIONS
tier_app_dirs += extensions
endif

# winEmbed
ifeq ($(OS_ARCH),WINNT)
ifneq (,$(ENABLE_TESTS)$(MOZILLA_OFFICIAL))
tier_app_dirs += embedding/tests/winEmbed
endif
endif

tier_app_dirs += xulrunner

installer:
	@echo "XULRunner doesn't have an installer yet."

package:
	@$(MAKE) -C xulrunner/installer

install:
	@$(MAKE) -C xulrunner/installer install

sdk:
	@$(MAKE) -C xulrunner/installer make-sdk

distclean::
	@$(MAKE) -C xulrunner/installer distclean

source-package::
	@$(MAKE) -C xulrunner/installer source-package

upload::
	@$(MAKE) -C xulrunner/installer upload

source-upload::
	@$(MAKE) -C xulrunner/installer source-upload

hg-bundle::
	@$(MAKE) -C xulrunner/installer hg-bundle

ifeq ($(OS_TARGET),Linux)
deb: package
	@$(MAKE) -C xulrunner/installer deb
endif
