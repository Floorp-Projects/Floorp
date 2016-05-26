# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# include automation-build.mk to get the path to the binary
TARGET_DEPTH = $(DEPTH)
include $(topsrcdir)/build/binary-location.mk

SYMBOLS_PATH := --symbols-path=$(DIST)/crashreporter-symbols

ifndef TEST_PACKAGE_NAME
TEST_PACKAGE_NAME := $(ANDROID_PACKAGE_NAME)
endif

# Linking xul-gtest.dll takes too long, so we disable GTest on
# Windows PGO builds (bug 1028035).
ifneq (1_WINNT,$(MOZ_PGO)_$(OS_ARCH))
BUILD_GTEST=1
endif

ifdef MOZ_B2G
BUILD_GTEST=
endif

ifneq (browser,$(MOZ_BUILD_APP))
BUILD_GTEST=
endif

ifndef COMPILE_ENVIRONMENT
BUILD_GTEST=
endif

ifndef NO_FAIL_ON_TEST_ERRORS
define check_test_error_internal
  @errors=`grep 'TEST-UNEXPECTED-' $@.log` ;\
  if test "$$errors" ; then \
	  echo '$@ failed:'; \
	  echo "$$errors"; \
          $(if $(1),echo $(1);) \
	  exit 1; \
  fi
endef
CHECK_TEST_ERROR = $(call check_test_error_internal)
CHECK_TEST_ERROR_RERUN = $(call check_test_error_internal,'To rerun your failures please run "make $@-rerun-failures"')
endif

# Usage: |make [EXTRA_TEST_ARGS=...] *test|.
RUN_REFTEST = rm -f ./$@.log && $(PYTHON) _tests/reftest/runreftest.py \
  --extra-profile-file=$(DIST)/plugins \
  $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(1) | tee ./$@.log

REMOTE_REFTEST = rm -f ./$@.log && $(PYTHON) _tests/reftest/remotereftest.py \
  --dm_trans=$(DM_TRANS) --ignore-window-size \
  --app=$(TEST_PACKAGE_NAME) --deviceIP=${TEST_DEVICE} --xre-path=${MOZ_HOST_BIN} \
  --httpd-path=_tests/modules --suite reftest \
  --extra-profile-file=$(topsrcdir)/mobile/android/fonts \
  $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(1) | tee ./$@.log

RUN_REFTEST_B2G = rm -f ./$@.log && $(PYTHON) _tests/reftest/runreftestb2g.py \
  --remote-webserver=10.0.2.2 --b2gpath=${B2G_PATH} --adbpath=${ADB_PATH} \
  --xre-path=${MOZ_HOST_BIN} $(SYMBOLS_PATH) --ignore-window-size \
  --httpd-path=_tests/modules \
  $(EXTRA_TEST_ARGS) '$(1)' | tee ./$@.log

ifeq ($(OS_ARCH),WINNT) #{
# GPU-rendered shadow layers are unsupported here
OOP_CONTENT = --setpref=layers.async-pan-zoom.enabled=true --setpref=browser.tabs.remote.autostart=true --setpref=layers.acceleration.disabled=true
GPU_RENDERING =
else
OOP_CONTENT = --setpref=layers.async-pan-zoom.enabled=true --setpref=browser.tabs.remote.autostart=true
GPU_RENDERING = --setpref=layers.acceleration.force-enabled=true
endif #}

reftest: TEST_PATH?=layout/reftests/reftest.list
reftest:
	$(call RUN_REFTEST,'$(topsrcdir)/$(TEST_PATH)')
	$(CHECK_TEST_ERROR)

reftest-remote: TEST_PATH?=layout/reftests/reftest.list
reftest-remote: DM_TRANS?=adb
reftest-remote:
	@if [ '${MOZ_HOST_BIN}' = '' ]; then \
        echo 'environment variable MOZ_HOST_BIN must be set to a directory containing host xpcshell'; \
    elif [ ! -d ${MOZ_HOST_BIN} ]; then \
        echo 'MOZ_HOST_BIN does not specify a directory'; \
    elif [ ! -f ${MOZ_HOST_BIN}/xpcshell ]; then \
        echo 'xpcshell not found in MOZ_HOST_BIN'; \
    elif [ '${TEST_DEVICE}' = '' -a '$(DM_TRANS)' != 'adb' ]; then \
        echo 'please prepare your host with the environment variable TEST_DEVICE'; \
    else \
        ln -s $(abspath $(topsrcdir)) _tests/reftest/tests; \
        $(call REMOTE_REFTEST,'tests/$(TEST_PATH)'); \
        $(CHECK_TEST_ERROR); \
    fi

reftest-b2g: TEST_PATH?=layout/reftests/reftest.list
reftest-b2g:
	@if [ '${MOZ_HOST_BIN}' = '' ]; then \
		echo 'environment variable MOZ_HOST_BIN must be set to a directory containing host xpcshell'; \
	elif [ ! -d ${MOZ_HOST_BIN} ]; then \
		echo 'MOZ_HOST_BIN does not specify a directory'; \
	elif [ ! -f ${MOZ_HOST_BIN}/xpcshell ]; then \
		echo 'xpcshell not found in MOZ_HOST_BIN'; \
	elif [ '${B2G_PATH}' = '' -o '${ADB_PATH}' = '' ]; then \
		echo 'please set the B2G_PATH and ADB_PATH environment variables'; \
	else \
        ln -s $(abspath $(topsrcdir)) _tests/reftest/tests; \
		if [ '${REFTEST_PATH}' != '' ]; then \
			$(call RUN_REFTEST_B2G,tests/${REFTEST_PATH}); \
		else \
			$(call RUN_REFTEST_B2G,tests/$(TEST_PATH)); \
		fi; \
        $(CHECK_TEST_ERROR); \
	fi

reftest-ipc: TEST_PATH?=layout/reftests/reftest.list
reftest-ipc:
	$(call RUN_REFTEST,'$(topsrcdir)/$(TEST_PATH)' $(OOP_CONTENT))
	$(CHECK_TEST_ERROR)

reftest-ipc-gpu: TEST_PATH?=layout/reftests/reftest.list
reftest-ipc-gpu:
	$(call RUN_REFTEST,'$(topsrcdir)/$(TEST_PATH)' $(OOP_CONTENT) $(GPU_RENDERING))
	$(CHECK_TEST_ERROR)

crashtest: TEST_PATH?=testing/crashtest/crashtests.list
crashtest:
	$(call RUN_REFTEST,'$(topsrcdir)/$(TEST_PATH)')
	$(CHECK_TEST_ERROR)

crashtest-ipc: TEST_PATH?=testing/crashtest/crashtests.list
crashtest-ipc:
	$(call RUN_REFTEST,'$(topsrcdir)/$(TEST_PATH)' $(OOP_CONTENT))
	$(CHECK_TEST_ERROR)

crashtest-ipc-gpu: TEST_PATH?=testing/crashtest/crashtests.list
crashtest-ipc-gpu:
	$(call RUN_REFTEST,'$(topsrcdir)/$(TEST_PATH)' $(OOP_CONTENT) $(GPU_RENDERING))
	$(CHECK_TEST_ERROR)

jstestbrowser: TESTS_PATH?=test-stage/jsreftest/tests/
jstestbrowser:
	$(MAKE) -C $(DEPTH)/config
	$(MAKE) stage-jstests
	$(call RUN_REFTEST,'$(DIST)/$(TESTS_PATH)/jstests.list' --extra-profile-file=$(DIST)/test-stage/jsreftest/tests/user.js)
	$(CHECK_TEST_ERROR)

GARBAGE += $(addsuffix .log,$(MOCHITESTS) reftest crashtest jstestbrowser)

REMOTE_CPPUNITTESTS = \
	$(PYTHON) -u $(topsrcdir)/testing/remotecppunittests.py \
	  --xre-path=$(DEPTH)/dist/bin \
	  --localLib=$(DEPTH)/dist/fennec \
	  --dm_trans=$(DM_TRANS) \
	  --deviceIP=${TEST_DEVICE} \
	  $(TEST_PATH) $(EXTRA_TEST_ARGS)

# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] cppunittests-remote|.
cppunittests-remote: DM_TRANS?=adb
cppunittests-remote:
	@if [ '${TEST_DEVICE}' != '' -o '$(DM_TRANS)' = 'adb' ]; \
          then $(call REMOTE_CPPUNITTESTS); \
        else \
          echo 'please prepare your host with environment variables for TEST_DEVICE'; \
        fi

jetpack-tests:
	cd $(topsrcdir)/addon-sdk/source && $(PYTHON) bin/cfx -b $(abspath $(browser_path)) --parseable testpkgs

pgo-profile-run:
	$(PYTHON) $(topsrcdir)/build/pgo/profileserver.py $(EXTRA_TEST_ARGS)

# Package up the tests and test harnesses
include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

PKG_STAGE = $(DIST)/test-stage

stage-all: \
  stage-config \
  stage-mach \
  stage-extensions \
  stage-mochitest \
  stage-jstests \
  stage-jetpack \
  stage-marionette \
  stage-luciddream \
  test-packages-manifest \
  $(NULL)
ifdef MOZ_WEBRTC
stage-all: stage-steeplechase
endif

ifdef COMPILE_ENVIRONMENT
stage-all: stage-cppunittests
endif

TEST_PKGS := \
  common \
  cppunittest \
  mochitest \
  reftest \
  talos \
  web-platform \
  xpcshell \
  $(NULL)

ifdef BUILD_GTEST
stage-all: stage-gtest
TEST_PKGS += gtest
endif

PKG_ARG = --$(1) '$(PKG_BASENAME).$(1).tests.zip'

test-packages-manifest:
	@rm -f $(MOZ_TEST_PACKAGES_FILE)
	$(NSINSTALL) -D $(dir $(MOZ_TEST_PACKAGES_FILE))
	$(PYTHON) $(topsrcdir)/build/gen_test_packages_manifest.py \
      --jsshell $(JSSHELL_NAME) \
      --dest-file '$(MOZ_TEST_PACKAGES_FILE)' \
      $(call PKG_ARG,common) \
      $(foreach pkg,$(TEST_PKGS),$(call PKG_ARG,$(pkg)))

package-tests-prepare-dest:
	@rm -f '$(DIST)/$(PKG_PATH)$(TEST_PACKAGE)'
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)

define package_archive
package-tests-$(1): stage-all package-tests-prepare-dest
	$$(call py_action,test_archive, \
		$(1) \
		'$$(abspath $$(DIST))/$$(PKG_PATH)/$$(PKG_BASENAME).$(1).tests.zip')
package-tests: package-tests-$(1)
endef

$(foreach name,$(TEST_PKGS),$(eval $(call package_archive,$(name))))

ifeq ($(MOZ_BUILD_APP),mobile/android)
stage-all: stage-android
stage-all: stage-instrumentation-tests
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),gonk)
stage-all: stage-b2g
endif

# Prepare _tests before any of the other staging/packaging steps.
# make-stage-dir is a prerequisite to all the stage-* targets in testsuite-targets.mk.
make-stage-dir: install-test-files
	rm -rf $(PKG_STAGE)
	$(NSINSTALL) -D $(PKG_STAGE)
	$(NSINSTALL) -D $(PKG_STAGE)/bin
	$(NSINSTALL) -D $(PKG_STAGE)/bin/components
	$(NSINSTALL) -D $(PKG_STAGE)/certs
	$(NSINSTALL) -D $(PKG_STAGE)/config
	$(NSINSTALL) -D $(PKG_STAGE)/jetpack
	$(NSINSTALL) -D $(PKG_STAGE)/modules
	$(NSINSTALL) -D $(PKG_STAGE)/tools/mach

stage-b2g: make-stage-dir
	$(NSINSTALL) $(topsrcdir)/b2g/test/b2g-unittest-requirements.txt $(PKG_STAGE)/b2g

stage-config: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/config
	@(cd $(topsrcdir)/testing/config && tar $(TAR_CREATE_FLAGS) - *) | (cd $(PKG_STAGE)/config && tar -xf -)

stage-mach: make-stage-dir
	@(cd $(topsrcdir)/python/mach && tar $(TAR_CREATE_FLAGS) - *) | (cd $(PKG_STAGE)/tools/mach && tar -xf -)
	cp $(topsrcdir)/testing/tools/mach_test_package_bootstrap.py $(PKG_STAGE)/tools/mach_bootstrap.py
	cp $(topsrcdir)/mach $(PKG_STAGE)

stage-mochitest: make-stage-dir
ifeq ($(MOZ_BUILD_APP),mobile/android)
	$(MAKE) -C $(DEPTH)/testing/mochitest stage-package
endif

stage-jstests: make-stage-dir
	$(MAKE) -C $(DEPTH)/js/src/tests stage-package

stage-gtest: make-stage-dir
# FIXME: (bug 1200311) We should be generating the gtest xul as part of the build.
	$(MAKE) -C $(DEPTH)/testing/gtest gtest
	$(NSINSTALL) -D $(PKG_STAGE)/gtest/gtest_bin
	cp -RL $(DIST)/bin/gtest $(PKG_STAGE)/gtest/gtest_bin
	cp -RL $(DEPTH)/_tests/gtest $(PKG_STAGE)
	cp $(topsrcdir)/testing/gtest/rungtests.py $(PKG_STAGE)/gtest
	cp $(DIST)/bin/dependentlibs.list.gtest $(PKG_STAGE)/gtest
	cp $(DEPTH)/mozinfo.json $(PKG_STAGE)/gtest

stage-android: make-stage-dir
ifdef MOZ_ENABLE_SZIP
# Tinderbox scripts are not unzipping everything, so the file needs to be in a directory it unzips
	$(NSINSTALL) $(DIST)/host/bin/szip $(PKG_STAGE)/bin/host
endif
	$(NSINSTALL) $(topsrcdir)/mobile/android/fonts $(DEPTH)/_tests/reftest
	$(NSINSTALL) $(topsrcdir)/mobile/android/fonts $(DEPTH)/_tests/testing/mochitest

stage-jetpack: make-stage-dir
	$(MAKE) -C $(DEPTH)/addon-sdk stage-tests-package

CPP_UNIT_TEST_BINS=$(wildcard $(DIST)/cppunittests/*)

ifdef OBJCOPY
ifneq ($(OBJCOPY), :) # see build/autoconf/toolchain.m4:102 for why this is necessary
ifndef PKG_SKIP_STRIP
STRIP_CPP_TESTS := 1
endif
endif
endif

stage-cppunittests: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/cppunittest
ifdef STRIP_CPP_TESTS
	$(foreach bin,$(CPP_UNIT_TEST_BINS),$(OBJCOPY) $(or $(STRIP_FLAGS),--strip-unneeded) $(bin) $(bin:$(DIST)/cppunittests/%=$(PKG_STAGE)/cppunittest/%);)
else
	cp -RL $(CPP_UNIT_TEST_BINS) $(PKG_STAGE)/cppunittest
endif
ifdef STRIP_CPP_TESTS
	$(OBJCOPY) $(or $(STRIP_FLAGS),--strip-unneeded) $(DIST)/bin/jsapi-tests$(BIN_SUFFIX) $(PKG_STAGE)/cppunittest/jsapi-tests$(BIN_SUFFIX)
else
	cp -RL $(DIST)/bin/jsapi-tests$(BIN_SUFFIX) $(PKG_STAGE)/cppunittest
endif

stage-steeplechase: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/steeplechase/
	cp -RL $(DEPTH)/_tests/steeplechase $(PKG_STAGE)/steeplechase/tests
	cp -RL $(DIST)/xpi-stage/specialpowers $(PKG_STAGE)/steeplechase
	cp -RL $(topsrcdir)/testing/profiles/prefs_general.js $(PKG_STAGE)/steeplechase

LUCIDDREAM_DIR=$(PKG_STAGE)/luciddream
stage-luciddream: make-stage-dir
	$(NSINSTALL) -D $(LUCIDDREAM_DIR)
	@(cd $(topsrcdir)/testing/luciddream && tar $(TAR_CREATE_FLAGS) - *) | (cd $(LUCIDDREAM_DIR)/ && tar -xf -)

MARIONETTE_DIR=$(PKG_STAGE)/marionette
stage-marionette: make-stage-dir
	$(NSINSTALL) -D $(MARIONETTE_DIR)/tests
	$(NSINSTALL) -D $(MARIONETTE_DIR)/client
	@(cd $(topsrcdir)/testing/marionette/harness && tar --exclude marionette/tests $(TAR_CREATE_FLAGS) - *) | (cd $(MARIONETTE_DIR)/ && tar -xf -)
	@(cd $(topsrcdir)/testing/marionette/client && tar $(TAR_CREATE_FLAGS) - *) | (cd $(MARIONETTE_DIR)/client && tar -xf -)
	$(PYTHON) $(topsrcdir)/testing/marionette/harness/marionette/tests/print-manifest-dirs.py \
          $(topsrcdir) \
          $(topsrcdir)/testing/marionette/harness/marionette/tests/unit-tests.ini \
          | (cd $(topsrcdir) && xargs tar $(TAR_CREATE_FLAGS) -) \
          | (cd $(MARIONETTE_DIR)/tests && tar -xf -)
	$(PYTHON) $(topsrcdir)/testing/marionette/harness/marionette/tests/print-manifest-dirs.py \
          $(topsrcdir) \
          $(topsrcdir)/testing/marionette/harness/marionette/tests/webapi-tests.ini \
          | (cd $(topsrcdir) && xargs tar $(TAR_CREATE_FLAGS) -) \
          | (cd $(MARIONETTE_DIR)/tests && tar -xf -)

stage-instrumentation-tests: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/instrumentation stage-package

TEST_EXTENSIONS := \
    specialpowers@mozilla.org.xpi \
	$(NULL)

stage-extensions: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/extensions/
	@$(foreach ext,$(TEST_EXTENSIONS), cp -RL $(DIST)/xpi-stage/$(ext) $(PKG_STAGE)/extensions;)

.PHONY: \
  reftest \
  crashtest \
  xpcshell-tests \
  jstestbrowser \
  package-tests \
  package-tests-prepare-dest \
  package-tests-common \
  make-stage-dir \
  stage-all \
  stage-b2g \
  stage-config \
  stage-mochitest \
  stage-jstests \
  stage-android \
  stage-jetpack \
  stage-marionette \
  stage-steeplechase \
  stage-instrumentation-tests \
  stage-luciddream \
  test-packages-manifest \
  $(NULL)
