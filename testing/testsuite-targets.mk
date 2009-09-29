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
#	Serge Gautherie <sgautherie.bz@free.fr>
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


# Shortcut for mochitest* and xpcshell-tests targets,
# replaces 'EXTRA_TEST_ARGS=--test-path=...'.
ifdef TEST_PATH
TEST_PATH_ARG := --test-path=$(TEST_PATH)
else
TEST_PATH_ARG :=
endif


# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] mochitest*|.
mochitest:: mochitest-plain mochitest-chrome mochitest-a11y

RUN_MOCHITEST = \
	rm -f ./$@.log && \
	$(PYTHON) _tests/testing/mochitest/runtests.py --autorun --close-when-done \
	  --console-level=INFO --log-file=./$@.log --file-level=INFO \
	  $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS)

ifndef NO_FAIL_ON_TEST_ERRORS
define CHECK_TEST_ERROR
  @errors=`grep "TEST-UNEXPECTED-" $@.log` ;\
  if test "$$errors" ; then \
	  echo "$@ failed:"; \
	  echo "$$errors"; \
	  exit 1; \
  else \
	  echo "$@ passed"; \
  fi
endef
endif

mochitest-plain:
	$(RUN_MOCHITEST)
	$(CHECK_TEST_ERROR)

mochitest-chrome:
	$(RUN_MOCHITEST) --chrome
	$(CHECK_TEST_ERROR)

mochitest-a11y:
	$(RUN_MOCHITEST) --a11y
	$(CHECK_TEST_ERROR)


# Usage: |make [EXTRA_TEST_ARGS=...] *test|.
RUN_REFTEST = rm -f ./$@.log && $(PYTHON) _tests/reftest/runreftest.py $(EXTRA_TEST_ARGS) $(1) | tee ./$@.log

reftest:
	$(call RUN_REFTEST,$(topsrcdir)/layout/reftests/reftest.list)
	$(CHECK_TEST_ERROR)

crashtest:
	$(call RUN_REFTEST,$(topsrcdir)/testing/crashtest/crashtests.list)
	$(CHECK_TEST_ERROR)


# Execute all xpcshell tests in the directories listed in the manifest.
# See also config/rules.mk 'xpcshell-tests' target for local execution.
# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] xpcshell-tests|.
xpcshell-tests:
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(topsrcdir)/build \
	  $(topsrcdir)/testing/xpcshell/runxpcshelltests.py \
	  --manifest=$(DEPTH)/_tests/xpcshell/all-test-dirs.list \
	  --no-logfiles \
	  --symbols-path=$(DIST)/crashreporter-symbols \
	  $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS) \
	  $(DIST)/bin/xpcshell


# Package up the tests and test harnesses
include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

PKG_STAGE = $(DIST)/test-package-stage

package-tests: stage-mochitest stage-reftest stage-xpcshell
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
	@(cd $(PKG_STAGE) && tar $(TAR_CREATE_FLAGS) - *) | bzip2 -f > $(DIST)/$(PKG_PATH)$(TEST_PACKAGE)

make-stage-dir:
	rm -rf $(PKG_STAGE) && $(NSINSTALL) -D $(PKG_STAGE) && $(NSINSTALL) -D $(PKG_STAGE)/bin && $(NSINSTALL) -D $(PKG_STAGE)/bin/components && $(NSINSTALL) -D $(PKG_STAGE)/certs

stage-mochitest: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/mochitest stage-package

stage-reftest: make-stage-dir
	$(MAKE) -C $(DEPTH)/layout/tools/reftest stage-package

stage-xpcshell: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/xpcshell stage-package


.PHONY: \
  mochitest mochitest-plain mochitest-chrome mochitest-a11y \
  reftest crashtest \
  xpcshell-tests \
  package-tests make-stage-dir stage-mochitest stage-reftest stage-xpcshell
