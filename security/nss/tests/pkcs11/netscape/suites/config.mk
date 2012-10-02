#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
all::

release:: release_security abort_rule

release_security:
	@echo "cd security; $(MAKE) release"
	$(MAKE) -C security release

abort_rule:
	@"Security Complete. (Don't worry about this, it really should abort here!)"
