#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
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
