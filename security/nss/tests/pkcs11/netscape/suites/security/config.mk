#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
all::

release:: release_des release_des3 release_sha1 release_dsa abort_rule

release_des:
	@echo "cd des; $(MAKE) release"
	$(MAKE) -C des release

release_des3:
	@echo "cd des3; $(MAKE) release"
	$(MAKE) -C des3 release

release_sha1:
	@echo "cd sha1; $(MAKE) release"
	$(MAKE) -C sha1 release

release_dsa:
	@echo "cd dsa; $(MAKE) release"
	$(MAKE) -C dsa release

abort_rule:
	@"Security Suites Complete. (Don't worry about this, it really should abort here!)"
