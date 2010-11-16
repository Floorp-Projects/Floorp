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

SYMBOLS_PATH := --symbols-path=$(DIST)/crashreporter-symbols

# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] mochitest*|.
MOCHITESTS := mochitest-plain mochitest-chrome mochitest-a11y mochitest-ipcplugins
mochitest:: $(MOCHITESTS)

RUN_MOCHITEST = \
	rm -f ./$@.log && \
	$(PYTHON) _tests/testing/mochitest/runtests.py --autorun --close-when-done \
	  --console-level=INFO --log-file=./$@.log --file-level=INFO \
	  $(SYMBOLS_PATH) $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS)

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

# Allow mochitest-1 ... mochitest-5 for developer ease
mochitest-1 mochitest-2 mochitest-3 mochitest-4 mochitest-5: mochitest-%:
	echo "mochitest: $* / 5"
	$(RUN_MOCHITEST) --chunk-by-dir=4 --total-chunks=5 --this-chunk=$*
	$(CHECK_TEST_ERROR)

mochitest-chrome:
	$(RUN_MOCHITEST) --chrome
	$(CHECK_TEST_ERROR)

mochitest-a11y:
	$(RUN_MOCHITEST) --a11y
	$(CHECK_TEST_ERROR)

mochitest-ipcplugins:
ifeq (Darwin,$(OS_ARCH))
ifeq (i386,$(TARGET_CPU))
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled.i386.test.plugin=true --test-path=modules/plugin/test
endif
ifeq (x86_64,$(TARGET_CPU))
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled.x86_64.test.plugin=true --test-path=modules/plugin/test
endif
ifeq (powerpc,$(TARGET_CPU))
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled.ppc.test.plugin=true --test-path=modules/plugin/test
endif
else
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled=true --test-path=modules/plugin/test
endif
	$(CHECK_TEST_ERROR)

# Usage: |make [EXTRA_TEST_ARGS=...] *test|.
RUN_REFTEST = rm -f ./$@.log && $(PYTHON) _tests/reftest/runreftest.py \
  $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(1) | tee ./$@.log

reftest: TEST_PATH?=layout/reftests/reftest.list
reftest:
	$(call RUN_REFTEST,$(topsrcdir)/$(TEST_PATH))
	$(CHECK_TEST_ERROR)

crashtest: TEST_PATH=testing/crashtest/crashtests.list
crashtest:
	$(call RUN_REFTEST,$(topsrcdir)/$(TEST_PATH))
	$(CHECK_TEST_ERROR)

jstestbrowser: TEST_PATH=js/src/tests/jstests.list
jstestbrowser:
	$(call RUN_REFTEST,$(topsrcdir)/$(TEST_PATH) --extra-profile-file=$(topsrcdir)/js/src/tests/user.js)
	$(CHECK_TEST_ERROR)

GARBAGE += $(addsuffix .log,$(MOCHITESTS) reftest crashtest jstestbrowser)

# Execute all xpcshell tests in the directories listed in the manifest.
# See also config/rules.mk 'xpcshell-tests' target for local execution.
# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] xpcshell-tests|.
xpcshell-tests:
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(topsrcdir)/build \
	  $(topsrcdir)/testing/xpcshell/runxpcshelltests.py \
	  --manifest=$(DEPTH)/_tests/xpcshell/all-test-dirs.list \
	  --no-logfiles \
          $(SYMBOLS_PATH) \
	  $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS) \
	  $(DIST)/bin/xpcshell

# Package up the tests and test harnesses
include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

ifndef UNIVERSAL_BINARY
PKG_STAGE = $(DIST)/test-package-stage
package-tests: stage-mochitest stage-reftest stage-xpcshell stage-jstests stage-mozmill stage-jetpack
else
# This staging area has been built for us by universal/flight.mk
PKG_STAGE = $(DIST)/universal/test-package-stage
endif

package-tests:
	@rm -f "$(DIST)/$(PKG_PATH)$(TEST_PACKAGE)"
ifndef UNIVERSAL_BINARY
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
else
	#building tests.jar (bug 543800) fails on unify, so we build tests.jar after unify is run
	$(MAKE) -C $(DEPTH)/testing/mochitest stage-chromejar PKG_STAGE=$(DIST)/universal
endif
	cd $(PKG_STAGE) && \
	  zip -r9D "$(call core_abspath,$(DIST)/$(PKG_PATH)$(TEST_PACKAGE))" *

ifeq (Android, $(OS_TARGET))
package-tests: stage-android
endif

make-stage-dir:
	rm -rf $(PKG_STAGE) && $(NSINSTALL) -D $(PKG_STAGE) && $(NSINSTALL) -D $(PKG_STAGE)/bin && $(NSINSTALL) -D $(PKG_STAGE)/bin/components && $(NSINSTALL) -D $(PKG_STAGE)/certs && $(NSINSTALL) -D $(PKG_STAGE)/jetpack

stage-mochitest: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/mochitest stage-package

stage-reftest: make-stage-dir
	$(MAKE) -C $(DEPTH)/layout/tools/reftest stage-package

stage-xpcshell: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/xpcshell stage-package

stage-jstests: make-stage-dir
	$(MAKE) -C $(DEPTH)/js/src/tests stage-package

stage-mozmill: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/mozmill stage-package

stage-android: make-stage-dir
	$(NSINSTALL) $(DEPTH)/build/mobile/sutagent/android/sutAgentAndroid.apk $(PKG_STAGE)/bin

stage-jetpack: make-stage-dir
	$(NSINSTALL) $(topsrcdir)/testing/jetpack/jetpack-location.txt $(PKG_STAGE)/jetpack
.PHONY: \
  mochitest mochitest-plain mochitest-chrome mochitest-a11y mochitest-ipcplugins \
  reftest crashtest \
  xpcshell-tests \
  jstestbrowser \
  package-tests make-stage-dir stage-mochitest stage-reftest stage-xpcshell stage-jstests stage-mozmill stage-android stage-jetpack
