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
  --ignore-window-size \
  --app=$(TEST_PACKAGE_NAME) --deviceIP=${TEST_DEVICE} --xre-path=${MOZ_HOST_BIN} \
  --httpd-path=_tests/modules --suite reftest \
  --extra-profile-file=$(topsrcdir)/mobile/android/fonts \
  $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(1) | tee ./$@.log

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
reftest-remote:
	@if [ '${MOZ_HOST_BIN}' = '' ]; then \
        echo 'environment variable MOZ_HOST_BIN must be set to a directory containing host xpcshell'; \
    elif [ ! -d ${MOZ_HOST_BIN} ]; then \
        echo 'MOZ_HOST_BIN does not specify a directory'; \
    elif [ ! -f ${MOZ_HOST_BIN}/xpcshell ]; then \
        echo 'xpcshell not found in MOZ_HOST_BIN'; \
    else \
        ln -s $(abspath $(topsrcdir)) _tests/reftest/tests; \
        $(call REMOTE_REFTEST,'tests/$(TEST_PATH)'); \
        $(CHECK_TEST_ERROR); \
    fi

crashtest: TEST_PATH?=testing/crashtest/crashtests.list
crashtest:
	$(call RUN_REFTEST,'$(topsrcdir)/$(TEST_PATH)')
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
	  --deviceIP=${TEST_DEVICE} \
	  $(TEST_PATH) $(EXTRA_TEST_ARGS)

# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] cppunittests-remote|.
cppunittests-remote:
	$(call REMOTE_CPPUNITTESTS);

# Package up the tests and test harnesses
include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

PKG_STAGE = $(DIST)/test-stage

stage-all: \
  stage-config \
  stage-mach \
  stage-extensions \
  stage-mochitest \
  stage-jstests \
  test-packages-manifest \
  $(NULL)
ifdef MOZ_WEBRTC
stage-all: stage-steeplechase
endif

ifdef COMPILE_ENVIRONMENT
stage-all: stage-cppunittests
endif

TEST_PKGS_ZIP := \
  common \
  cppunittest \
  mochitest \
  reftest \
  talos \
  raptor \
  awsy \
  xpcshell \
  $(NULL)

TEST_PKGS_TARGZ := \
  web-platform \
  $(NULL)

ifdef LINK_GTEST_DURING_COMPILE
stage-all: stage-gtest
TEST_PKGS_ZIP += gtest
endif

PKG_ARG = --$(1) '$(PKG_BASENAME).$(1).tests.$(2)'

test-packages-manifest:
	@rm -f $(MOZ_TEST_PACKAGES_FILE)
	$(NSINSTALL) -D $(dir $(MOZ_TEST_PACKAGES_FILE))
	$(PYTHON) $(topsrcdir)/build/gen_test_packages_manifest.py \
      --jsshell $(JSSHELL_NAME) \
      --dest-file '$(MOZ_TEST_PACKAGES_FILE)' \
      $(call PKG_ARG,common,zip) \
      $(foreach pkg,$(TEST_PKGS_ZIP),$(call PKG_ARG,$(pkg),zip)) \
      $(foreach pkg,$(TEST_PKGS_TARGZ),$(call PKG_ARG,$(pkg),tar.gz))

ifdef UPLOAD_PATH
test_archive_dir = $(UPLOAD_PATH)
else
test_archive_dir = $(DIST)/$(PKG_PATH)
endif

package-tests-prepare-dest:
	$(NSINSTALL) -D $(test_archive_dir)

define package_archive
package-tests-$(1): stage-all package-tests-prepare-dest
	$$(call py_action,test_archive, \
		$(1) \
		'$$(abspath $$(test_archive_dir))/$$(PKG_BASENAME).$(1).tests.$(2)')
package-tests: package-tests-$(1)
endef

$(foreach name,$(TEST_PKGS_ZIP),$(eval $(call package_archive,$(name),zip)))
$(foreach name,$(TEST_PKGS_TARGZ),$(eval $(call package_archive,$(name),tar.gz)))

ifeq ($(MOZ_BUILD_APP),mobile/android)
stage-all: stage-android
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
	$(NSINSTALL) -D $(PKG_STAGE)/modules
	$(NSINSTALL) -D $(PKG_STAGE)/tools/mach

stage-config: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/config
	@(cd $(topsrcdir)/testing/config && tar $(TAR_CREATE_FLAGS) - *) | (cd $(PKG_STAGE)/config && tar -xf -)

stage-mach: make-stage-dir
	@(cd $(topsrcdir)/python/mach && tar $(TAR_CREATE_FLAGS) - *) | (cd $(PKG_STAGE)/tools/mach && tar -xf -)
	cp $(topsrcdir)/testing/tools/mach_test_package_bootstrap.py $(PKG_STAGE)/tools/mach_bootstrap.py
	cp $(topsrcdir)/mach $(PKG_STAGE)

stage-mochitest: make-stage-dir ;

stage-jstests: make-stage-dir
	$(MAKE) -C $(DEPTH)/js/src/tests stage-package

ifdef OBJCOPY
ifneq ($(OBJCOPY), :) # see build/autoconf/toolchain.m4:102 for why this is necessary
ifndef PKG_SKIP_STRIP
STRIP_COMPILED_TESTS := 1
endif
endif
endif

stage-gtest: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/gtest/gtest_bin/gtest
ifdef STRIP_COMPILED_TESTS
# The libxul file basename will vary per platform. Fortunately
# dependentlibs.list always lists the library name as its final line, so we
# can get the value from there.
	LIBXUL_BASE=`tail -1 $(DIST)/bin/dependentlibs.list` && \
        $(OBJCOPY) $(or $(STRIP_FLAGS),--strip-unneeded) \
        $(DIST)/bin/gtest/$$LIBXUL_BASE $(PKG_STAGE)/gtest/gtest_bin/gtest/$$LIBXUL_BASE
else
	cp -RL $(DIST)/bin/gtest $(PKG_STAGE)/gtest/gtest_bin
endif
	cp -RL $(DEPTH)/_tests/gtest $(PKG_STAGE)
	cp $(topsrcdir)/testing/gtest/rungtests.py $(PKG_STAGE)/gtest
	cp $(DIST)/bin/dependentlibs.list.gtest $(PKG_STAGE)/gtest
	cp $(DEPTH)/mozinfo.json $(PKG_STAGE)/gtest

stage-android: make-stage-dir
	$(NSINSTALL) $(topsrcdir)/mobile/android/fonts $(DEPTH)/_tests/reftest
	$(NSINSTALL) $(topsrcdir)/mobile/android/fonts $(DEPTH)/_tests/testing/mochitest
	$(NSINSTALL) -D $(DEPTH)/_tests/reftest/hyphenation
	$(NSINSTALL) $(wildcard $(topsrcdir)/intl/locales/*/hyphenation/*.dic) $(DEPTH)/_tests/reftest/hyphenation

CPP_UNIT_TEST_BINS=$(wildcard $(DIST)/cppunittests/*)

stage-cppunittests: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/cppunittest
ifdef STRIP_COMPILED_TESTS
	$(foreach bin,$(CPP_UNIT_TEST_BINS),$(OBJCOPY) $(or $(STRIP_FLAGS),--strip-unneeded) $(bin) $(bin:$(DIST)/cppunittests/%=$(PKG_STAGE)/cppunittest/%);)
else
	cp -RL $(CPP_UNIT_TEST_BINS) $(PKG_STAGE)/cppunittest
endif
ifdef STRIP_COMPILED_TESTS
	$(OBJCOPY) $(or $(STRIP_FLAGS),--strip-unneeded) $(DIST)/bin/jsapi-tests$(BIN_SUFFIX) $(PKG_STAGE)/cppunittest/jsapi-tests$(BIN_SUFFIX)
else
	cp -RL $(DIST)/bin/jsapi-tests$(BIN_SUFFIX) $(PKG_STAGE)/cppunittest
endif

stage-steeplechase: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/steeplechase/
	cp -RL $(DEPTH)/_tests/steeplechase $(PKG_STAGE)/steeplechase/tests
	cp -RL $(DIST)/xpi-stage/specialpowers $(PKG_STAGE)/steeplechase
	cp -RL $(topsrcdir)/testing/profiles/common/user.js $(PKG_STAGE)/steeplechase/prefs_general.js

TEST_EXTENSIONS := \
    specialpowers@mozilla.org.xpi \
	$(NULL)

stage-extensions: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/extensions/
	@$(foreach ext,$(TEST_EXTENSIONS), cp -RL $(DIST)/xpi-stage/$(ext) $(PKG_STAGE)/extensions;)


check::
	$(eval cores=$(shell $(PYTHON) -c 'import multiprocessing; print(multiprocessing.cpu_count())'))
	@echo "Starting 'mach python-test' with -j$(cores)"
	@$(topsrcdir)/mach --log-no-times python-test -j$(cores) --subsuite default
	@echo "Finished 'mach python-test' successfully"


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
  stage-config \
  stage-mochitest \
  stage-jstests \
  stage-android \
  stage-steeplechase \
  test-packages-manifest \
  check \
  $(NULL)
