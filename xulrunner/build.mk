# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

installer:
	@echo 'XULRunner doesn't have an installer yet.'

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
