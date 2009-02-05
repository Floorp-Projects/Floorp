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
# The Original Code is Mozilla Test Harnesses
#
# The Initial Developer of the Original Code is
# The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#	Ted Mielczarek <ted.mielczarek@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

mochitest:: mochitest-plain mochitest-chrome mochitest-a11y

RUN_MOCHITEST = rm -f ./test-output.log && $(PYTHON) _tests/testing/mochitest/runtests.py --autorun --close-when-done --console-level=INFO  --log-file=./test-output.log --file-level=INFO

ifndef NO_FAIL_ON_TEST_ERRORS
define CHECK_TEST_ERROR
  @errors=`grep "TEST-UNEXPECTED-" test-output.log` ;\
  if test "$$errors" ; then \
	  echo "$@ failed:"; \
	  echo "$$errors"; \
	  exit 1; \
  else \
	  echo "$@ passed"; \
  fi
endef
endif

ifdef TEST_PATH
MOCHITEST_PATH = --test-path=$(TEST_PATH)
else
MOCHITEST_PATH =
endif

mochitest-plain:
	$(RUN_MOCHITEST) $(MOCHITEST_PATH)
	$(CHECK_TEST_ERROR)

mochitest-chrome:
	$(RUN_MOCHITEST) --chrome $(MOCHITEST_PATH)
	$(CHECK_TEST_ERROR)

mochitest-a11y:
	$(RUN_MOCHITEST) --a11y $(MOCHITEST_PATH)
	$(CHECK_TEST_ERROR)

# Package up the tests and test harnesses
include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

PKG_STAGE = $(DIST)/test-package-stage

package-tests: stage-mochitest
	@(cd $(PKG_STAGE) && tar $(TAR_CREATE_FLAGS) - *) | bzip2 -f > $(DIST)/$(PKG_PATH)$(TEST_PACKAGE)

make-stage-dir:
	rm -rf $(PKG_STAGE) && $(NSINSTALL) -D $(PKG_STAGE) && $(NSINSTALL) -D $(PKG_STAGE)/bin && $(NSINSTALL) -D $(PKG_STAGE)/bin/components && $(NSINSTALL) -D $(PKG_STAGE)/certs

stage-mochitest: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/mochitest stage-package

.PHONY: mochitest mochitest-plain mochitest-chrome mochitest-a11y \
  package-tests make-stage-dir stage-mochitest
