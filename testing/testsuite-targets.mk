# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Shortcut for mochitest* and xpcshell-tests targets
ifdef TEST_PATH
TEST_PATH_ARG := '$(TEST_PATH)'
IPCPLUGINS_PATH_ARG := '$(TEST_PATH)'
else
TEST_PATH_ARG :=
IPCPLUGINS_PATH_ARG := dom/plugins/test
endif

# include automation-build.mk to get the path to the binary
TARGET_DEPTH = $(DEPTH)
include $(topsrcdir)/build/binary-location.mk

SYMBOLS_PATH := --symbols-path=$(DIST)/crashreporter-symbols

# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] mochitest*|.
MOCHITESTS := mochitest-plain mochitest-chrome mochitest-devtools mochitest-a11y mochitest-ipcplugins
mochitest:: $(MOCHITESTS)

ifndef TEST_PACKAGE_NAME
TEST_PACKAGE_NAME := $(ANDROID_PACKAGE_NAME)
endif

RUN_MOCHITEST_B2G_DESKTOP = \
  rm -f ./$@.log && \
  $(PYTHON) _tests/testing/mochitest/runtestsb2g.py \
    --log-tbpl=./$@.log \
    --desktop --profile ${GAIA_PROFILE_DIR} \
    --failure-file=$(abspath _tests/testing/mochitest/makefailures.json) \
    $(EXTRA_TEST_ARGS) $(TEST_PATH_ARG)

RUN_MOCHITEST = \
  rm -f ./$@.log && \
  $(PYTHON) _tests/testing/mochitest/runtests.py \
    --log-tbpl=./$@.log \
    --failure-file=$(abspath _tests/testing/mochitest/makefailures.json) \
    --testing-modules-dir=$(abspath _tests/modules) \
    $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(TEST_PATH_ARG)

RERUN_MOCHITEST = \
  rm -f ./$@.log && \
  $(PYTHON) _tests/testing/mochitest/runtests.py \
    --log-tbpl=./$@.log \
    --run-only-tests=makefailures.json \
    --testing-modules-dir=$(abspath _tests/modules) \
    $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(TEST_PATH_ARG)

RUN_MOCHITEST_REMOTE = \
  rm -f ./$@.log && \
  $(PYTHON) _tests/testing/mochitest/runtestsremote.py \
    --log-tbpl=./$@.log $(DM_FLAGS) --dm_trans=$(DM_TRANS) \
    --app=$(TEST_PACKAGE_NAME) --deviceIP=${TEST_DEVICE} --xre-path=${MOZ_HOST_BIN} \
    --testing-modules-dir=$(abspath _tests/modules) \
    $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(TEST_PATH_ARG)

RUN_MOCHITEST_ROBOCOP = \
  rm -f ./$@.log && \
  $(PYTHON) _tests/testing/mochitest/runtestsremote.py \
    --robocop-apk=$(DEPTH)/build/mobile/robocop/robocop-debug.apk \
    --robocop-ids=$(DEPTH)/mobile/android/base/fennec_ids.txt \
    --robocop-ini=_tests/testing/mochitest/robocop.ini \
    --log-tbpl=./$@.log $(DM_FLAGS) --dm_trans=$(DM_TRANS) \
    --app=$(TEST_PACKAGE_NAME) --deviceIP=${TEST_DEVICE} --xre-path=${MOZ_HOST_BIN} \
    $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(TEST_PATH_ARG)

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

mochitest-remote: DM_TRANS?=adb
mochitest-remote:
	@if [ '${MOZ_HOST_BIN}' = '' ]; then \
        echo 'environment variable MOZ_HOST_BIN must be set to a directory containing host xpcshell'; \
    elif [ ! -d ${MOZ_HOST_BIN} ]; then \
        echo 'MOZ_HOST_BIN does not specify a directory'; \
    elif [ ! -f ${MOZ_HOST_BIN}/xpcshell ]; then \
        echo 'xpcshell not found in MOZ_HOST_BIN'; \
    elif [ '${TEST_DEVICE}' = '' -a '$(DM_TRANS)' != 'adb' ]; then \
        echo 'please prepare your host with the environment variable TEST_DEVICE'; \
    else \
        $(RUN_MOCHITEST_REMOTE); \
    fi

mochitest-robotium: mochitest-robocop
	@echo 'mochitest-robotium is deprecated -- please use mochitest-robocop'

mochitest-robocop: DM_TRANS?=adb
mochitest-robocop:
	@if [ '${MOZ_HOST_BIN}' = '' ]; then \
        echo 'environment variable MOZ_HOST_BIN must be set to a directory containing host xpcshell'; \
    elif [ ! -d ${MOZ_HOST_BIN} ]; then \
        echo 'MOZ_HOST_BIN does not specify a directory'; \
    elif [ ! -f ${MOZ_HOST_BIN}/xpcshell ]; then \
        echo 'xpcshell not found in MOZ_HOST_BIN'; \
    elif [ '${TEST_DEVICE}' = '' -a '$(DM_TRANS)' != 'adb' ]; then \
        echo 'please prepare your host with the environment variable TEST_DEVICE'; \
    else \
        $(RUN_MOCHITEST_ROBOCOP); \
    fi

ifdef MOZ_B2G
mochitest-plain:
	@if [ '${GAIA_PROFILE_DIR}'  = '' ]; then \
        echo 'please specify the GAIA_PROFILE_DIR env variable'; \
    else \
        $(RUN_MOCHITEST_B2G_DESKTOP); \
        $(CHECK_TEST_ERROR_RERUN); \
    fi
else
mochitest-plain:
	$(RUN_MOCHITEST)
	$(CHECK_TEST_ERROR_RERUN)
endif

mochitest-plain-rerun-failures:
	$(RERUN_MOCHITEST)
	$(CHECK_TEST_ERROR_RERUN)

# Allow mochitest-1 ... mochitest-5 for developer ease
mochitest-1 mochitest-2 mochitest-3 mochitest-4 mochitest-5: mochitest-%:
	echo 'mochitest: $* / 5'
	$(RUN_MOCHITEST) --chunk-by-dir=4 --total-chunks=5 --this-chunk=$*
	$(CHECK_TEST_ERROR)

mochitest-chrome:
	$(RUN_MOCHITEST) --chrome
	$(CHECK_TEST_ERROR)

mochitest-devtools:
	$(RUN_MOCHITEST) --subsuite=devtools
	$(CHECK_TEST_ERROR)

mochitest-a11y:
	$(RUN_MOCHITEST) --a11y
	$(CHECK_TEST_ERROR)

mochitest-ipcplugins:
ifeq (Darwin,$(OS_ARCH))
ifeq (i386,$(TARGET_CPU))
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled.i386.test.plugin=false $(IPCPLUGINS_PATH_ARG)
endif
ifeq (x86_64,$(TARGET_CPU))
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled.x86_64.test.plugin=false $(IPCPLUGINS_PATH_ARG)
endif
ifeq (powerpc,$(TARGET_CPU))
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled.ppc.test.plugin=false $(IPCPLUGINS_PATH_ARG)
endif
else
	$(RUN_MOCHITEST) --setpref=dom.ipc.plugins.enabled=false dom/plugins/test
endif
	$(CHECK_TEST_ERROR)

ifeq ($(OS_ARCH),Darwin)
webapprt_stub_path = $(TARGET_DIST)/$(MOZ_MACBUNDLE_NAME)/Contents/Resources/webapprt-stub$(BIN_SUFFIX)
endif
ifeq ($(OS_ARCH),WINNT)
webapprt_stub_path = $(TARGET_DIST)/bin/webapprt-stub$(BIN_SUFFIX)
endif
ifeq ($(MOZ_WIDGET_TOOLKIT),gtk2)
webapprt_stub_path = $(TARGET_DIST)/bin/webapprt-stub$(BIN_SUFFIX)
endif

ifdef webapprt_stub_path
webapprt-test-content:
	$(RUN_MOCHITEST) --webapprt-content --appname $(webapprt_stub_path)
	$(CHECK_TEST_ERROR)
webapprt-test-chrome:
	$(RUN_MOCHITEST) --webapprt-chrome --appname $(webapprt_stub_path) --browser-arg -test-mode
	$(CHECK_TEST_ERROR)
endif

# Usage: |make [EXTRA_TEST_ARGS=...] *test|.
RUN_REFTEST = rm -f ./$@.log && $(PYTHON) _tests/reftest/runreftest.py \
  --extra-profile-file=$(DIST)/plugins \
  $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) $(1) | tee ./$@.log

REMOTE_REFTEST = rm -f ./$@.log && $(PYTHON) _tests/reftest/remotereftest.py \
  --dm_trans=$(DM_TRANS) --ignore-window-size \
  --app=$(TEST_PACKAGE_NAME) --deviceIP=${TEST_DEVICE} --xre-path=${MOZ_HOST_BIN} \
  --httpd-path=_tests/modules \
  $(SYMBOLS_PATH) $(EXTRA_TEST_ARGS) '$(1)' | tee ./$@.log

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
        $(call REMOTE_REFTEST,tests/$(TEST_PATH)); \
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

ifeq ($(OS_ARCH),Darwin)
xpcshell_path = $(TARGET_DIST)/$(MOZ_MACBUNDLE_NAME)/Contents/MacOS/xpcshell
else
xpcshell_path = $(LIBXUL_DIST)/bin/xpcshell
endif

# Execute all xpcshell tests in the directories listed in the manifest.
# See also config/rules.mk 'xpcshell-tests' target for local execution.
# Usage: |make [TEST_PATH=...] [EXTRA_TEST_ARGS=...] xpcshell-tests|.
xpcshell-tests:
	$(info Have you considered running xpcshell tests via |mach xpcshell-test|? mach is easier to use and has more features than make and it will eventually be the only way to run xpcshell tests. Please consider using mach today!)
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(DEPTH)/build \
	  -I$(topsrcdir)/build \
	  -I$(DEPTH)/_tests/mozbase/mozinfo \
	  $(topsrcdir)/testing/xpcshell/runxpcshelltests.py \
	  --manifest=$(DEPTH)/_tests/xpcshell/xpcshell.ini \
	  --build-info-json=$(DEPTH)/mozinfo.json \
	  --no-logfiles \
	  --test-plugin-path='$(DIST)/plugins' \
	  --tests-root-dir=$(abspath _tests/xpcshell) \
	  --testing-modules-dir=$(abspath _tests/modules) \
          $(SYMBOLS_PATH) \
	  $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS) \
	  $(xpcshell_path)

B2G_XPCSHELL = \
	rm -f ./@.log && \
	$(PYTHON) -u $(topsrcdir)/config/pythonpath.py \
	  -I$(DEPTH)/build \
	  -I$(topsrcdir)/build \
	  $(topsrcdir)/testing/xpcshell/runtestsb2g.py \
	  --manifest=$(DEPTH)/_tests/xpcshell/xpcshell.ini \
	  --build-info-json=$(DEPTH)/mozinfo.json \
	  --no-logfiles \
	  --use-device-libs \
	  --no-clean \
	  --objdir=$(DEPTH) \
	  $$EXTRA_XPCSHELL_ARGS \
	  --b2gpath=${B2G_HOME} \
	  $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS)

xpcshell-tests-b2g: ADB_PATH?=$(shell which adb)
xpcshell-tests-b2g:
	@if [ '${B2G_HOME}' = '' ]; then \
		echo 'Please set the B2G_HOME variable'; exit 1; \
	elif [ ! -f '${ADB_PATH}' ]; then \
		echo 'Please set the ADB_PATH variable'; exit 1; \
	elif [ '${EMULATOR}' != '' ]; then \
		EXTRA_XPCSHELL_ARGS=--emulator=${EMULATOR}; \
		$(call B2G_XPCSHELL); \
		exit 0; \
	else \
		EXTRA_XPCSHELL_ARGS=--address=localhost:2828; \
		$(call B2G_XPCSHELL); \
		exit 0; \
	fi

xpcshell-tests-remote: DM_TRANS?=adb
xpcshell-tests-remote:
	@if [ '${TEST_DEVICE}' != '' -o '$(DM_TRANS)' = 'adb' ]; \
          then $(PYTHON) -u $(topsrcdir)/testing/xpcshell/remotexpcshelltests.py \
	    --manifest=$(DEPTH)/_tests/xpcshell/xpcshell_android.ini \
	    --build-info-json=$(DEPTH)/mozinfo.json \
	    --no-logfiles \
	    --testing-modules-dir=$(abspath _tests/modules) \
	    --dm_trans=$(DM_TRANS) \
	    --deviceIP=${TEST_DEVICE} \
	    --objdir=$(DEPTH) \
	    $(SYMBOLS_PATH) \
	    $(TEST_PATH_ARG) $(EXTRA_TEST_ARGS); \
	    $(CHECK_TEST_ERROR); \
        else \
          echo 'please prepare your host with environment variables for TEST_DEVICE'; \
        fi

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

ifndef UNIVERSAL_BINARY
PKG_STAGE = $(DIST)/test-stage
package-tests: \
  stage-config \
  stage-mach \
  stage-mochitest \
  stage-reftest \
  stage-xpcshell \
  stage-jstests \
  stage-jetpack \
  stage-mozbase \
  stage-tps \
  stage-modules \
  stage-marionette \
  stage-cppunittests \
  stage-jittest \
  stage-web-platform-tests \
  stage-luciddream \
  test-packages-manifest \
  test-packages-manifest-tc \
  $(NULL)
ifdef MOZ_WEBRTC
package-tests: stage-steeplechase
endif
else
# This staging area has been built for us by universal/flight.mk
PKG_STAGE = $(DIST)/universal/test-stage
endif

TEST_PKGS := \
  cppunittest \
  mochitest \
  reftest \
  xpcshell \
  web-platform \
  $(NULL)

PKG_ARG = --$(1) '$(PKG_BASENAME).$(1).tests.zip'

test-packages-manifest-tc:
	$(PYTHON) $(topsrcdir)/build/gen_test_packages_manifest.py \
      --jsshell $(JSSHELL_NAME) \
      --dest-file $(MOZ_TEST_PACKAGES_FILE_TC) \
      --use-short-names \
      $(call PKG_ARG,common) \
      $(foreach pkg,$(TEST_PKGS),$(call PKG_ARG,$(pkg)))

test-packages-manifest:
	@rm -f $(MOZ_TEST_PACKAGES_FILE)
	$(PYTHON) $(topsrcdir)/build/gen_test_packages_manifest.py \
      --jsshell $(JSSHELL_NAME) \
      --dest-file $(MOZ_TEST_PACKAGES_FILE) \
      $(call PKG_ARG,common) \
      $(foreach pkg,$(TEST_PKGS),$(call PKG_ARG,$(pkg)))

package-tests:
	@rm -f '$(DIST)/$(PKG_PATH)$(TEST_PACKAGE)'
ifndef UNIVERSAL_BINARY
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
endif
# Exclude harness specific directories when generating the common zip.
	$(MKDIR) -p $(abspath $(DIST))/$(PKG_PATH) && \
	cd $(PKG_STAGE) && \
	  zip -rq9D '$(abspath $(DIST))/$(PKG_PATH)$(TEST_PACKAGE)' \
	  * -x \*/.mkdir.done \*.pyc $(foreach name,$(TEST_PKGS),$(name)\*) && \
	$(foreach name,$(TEST_PKGS),rm -f '$(DIST)/$(PKG_PATH)$(PKG_BASENAME).'$(name)'.tests.zip' && \
                                zip -rq9D '$(abspath $(DIST))/$(PKG_PATH)$(PKG_BASENAME).'$(name)'.tests.zip' \
                                $(name) -x \*/.mkdir.done \*.pyc ;)

ifeq ($(MOZ_WIDGET_TOOLKIT),android)
package-tests: stage-android
package-tests: stage-instrumentation-tests
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),gonk)
package-tests: stage-b2g
endif

make-stage-dir:
	rm -rf $(PKG_STAGE)
	$(NSINSTALL) -D $(PKG_STAGE)
	$(NSINSTALL) -D $(PKG_STAGE)/bin
	$(NSINSTALL) -D $(PKG_STAGE)/bin/components
	$(NSINSTALL) -D $(PKG_STAGE)/certs
	$(NSINSTALL) -D $(PKG_STAGE)/config
	$(NSINSTALL) -D $(PKG_STAGE)/jetpack
	$(NSINSTALL) -D $(PKG_STAGE)/mozbase
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
	$(MAKE) -C $(DEPTH)/testing/mochitest stage-package
ifeq ($(MOZ_BUILD_APP),mobile/android)
	$(NSINSTALL) $(DEPTH)/mobile/android/base/fennec_ids.txt $(PKG_STAGE)/mochitest
endif

stage-reftest: make-stage-dir
	$(MAKE) -C $(DEPTH)/layout/tools/reftest stage-package

stage-xpcshell: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/xpcshell stage-package

stage-jstests: make-stage-dir
	$(MAKE) -C $(DEPTH)/js/src/tests stage-package

stage-android: make-stage-dir
ifdef MOZ_ENABLE_SZIP
# Tinderbox scripts are not unzipping everything, so the file needs to be in a directory it unzips
	$(NSINSTALL) $(DIST)/host/bin/szip $(PKG_STAGE)/bin/host
endif
	$(NSINSTALL) $(DEPTH)/build/mobile/sutagent/android/sutAgentAndroid.apk $(PKG_STAGE)/bin
	$(NSINSTALL) $(DEPTH)/build/mobile/sutagent/android/watcher/Watcher.apk $(PKG_STAGE)/bin
	$(NSINSTALL) $(DEPTH)/build/mobile/sutagent/android/fencp/FenCP.apk $(PKG_STAGE)/bin
	$(NSINSTALL) $(DEPTH)/build/mobile/sutagent/android/ffxcp/FfxCP.apk $(PKG_STAGE)/bin

stage-jetpack: make-stage-dir
	$(MAKE) -C $(DEPTH)/addon-sdk stage-tests-package

stage-tps: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/tps/tests
	@(cd $(topsrcdir)/testing/tps && tar $(TAR_CREATE_FLAGS) - *) | (cd $(PKG_STAGE)/tps && tar -xf -)
	@(cd $(topsrcdir)/services/sync/tps && tar $(TAR_CREATE_FLAGS) - *) | (cd $(PKG_STAGE)/tps && tar -xf -)
	(cd $(topsrcdir)/services/sync/tests/tps && tar $(TAR_CREATE_FLAGS) - *) | (cd $(PKG_STAGE)/tps/tests && tar -xf -)

stage-modules: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/modules
	cp -RL $(DEPTH)/_tests/modules $(PKG_STAGE)

CPP_UNIT_TEST_BINS=$(wildcard $(DIST)/cppunittests/*)

ifdef OBJCOPY
ifndef PKG_SKIP_STRIP
STRIP_CPP_TESTS := 1
endif
endif

stage-cppunittests: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/cppunittest
ifdef STRIP_CPP_TESTS
	$(foreach bin,$(CPP_UNIT_TEST_BINS),$(OBJCOPY) $(or $(STRIP_FLAGS),--strip-unneeded) $(bin) $(bin:$(DIST)/cppunittests/%=$(PKG_STAGE)/cppunittest/%);)
else
	cp -RL $(CPP_UNIT_TEST_BINS) $(PKG_STAGE)/cppunittest
endif
	cp $(topsrcdir)/testing/runcppunittests.py $(PKG_STAGE)/cppunittest
	cp $(topsrcdir)/testing/remotecppunittests.py $(PKG_STAGE)/cppunittest
	cp $(topsrcdir)/testing/cppunittest.ini $(PKG_STAGE)/cppunittest
	cp $(DEPTH)/mozinfo.json $(PKG_STAGE)/cppunittest
ifeq ($(MOZ_DISABLE_STARTUPCACHE),)
	cp $(topsrcdir)/startupcache/test/TestStartupCacheTelemetry.js $(PKG_STAGE)/cppunittest
	cp $(topsrcdir)/startupcache/test/TestStartupCacheTelemetry.manifest $(PKG_STAGE)/cppunittest
endif
ifdef STRIP_CPP_TESTS
	$(OBJCOPY) $(or $(STRIP_FLAGS),--strip-unneeded) $(DIST)/bin/jsapi-tests$(BIN_SUFFIX) $(PKG_STAGE)/cppunittest/jsapi-tests$(BIN_SUFFIX)
else
	cp -RL $(DIST)/bin/jsapi-tests$(BIN_SUFFIX) $(PKG_STAGE)/cppunittest
endif

stage-jittest: make-stage-dir
	$(NSINSTALL) -D $(PKG_STAGE)/jit-test/tests
	cp -RL $(topsrcdir)/js/src/jsapi.h $(PKG_STAGE)/jit-test/
	cp -RL $(topsrcdir)/js/src/jit-test $(PKG_STAGE)/jit-test/
	cp -RL $(topsrcdir)/js/src/tests/ecma_6 $(PKG_STAGE)/jit-test/tests/
	cp -RL $(topsrcdir)/js/src/tests/js1_8_5 $(PKG_STAGE)/jit-test/tests/
	cp -RL $(topsrcdir)/js/src/tests/lib $(PKG_STAGE)/jit-test/tests/

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
	$(NSINSTALL) -D $(MARIONETTE_DIR)/transport
	$(NSINSTALL) -D $(MARIONETTE_DIR)/driver
	@(cd $(topsrcdir)/testing/marionette/client && tar --exclude marionette/tests $(TAR_CREATE_FLAGS) - *) | (cd $(MARIONETTE_DIR)/ && tar -xf -)
	@(cd $(topsrcdir)/testing/marionette/transport && tar $(TAR_CREATE_FLAGS) - *) | (cd $(MARIONETTE_DIR)/transport && tar -xf -)
	@(cd $(topsrcdir)/testing/marionette/driver && tar $(TAR_CREATE_FLAGS) - *) | (cd $(MARIONETTE_DIR)/driver && tar -xf -)
	$(PYTHON) $(topsrcdir)/testing/marionette/client/marionette/tests/print-manifest-dirs.py \
          $(topsrcdir) \
          $(topsrcdir)/testing/marionette/client/marionette/tests/unit-tests.ini \
          | (cd $(topsrcdir) && xargs tar $(TAR_CREATE_FLAGS) -) \
          | (cd $(MARIONETTE_DIR)/tests && tar -xf -)
	$(PYTHON) $(topsrcdir)/testing/marionette/client/marionette/tests/print-manifest-dirs.py \
          $(topsrcdir) \
          $(topsrcdir)/testing/marionette/client/marionette/tests/webapi-tests.ini \
          | (cd $(topsrcdir) && xargs tar $(TAR_CREATE_FLAGS) -) \
          | (cd $(MARIONETTE_DIR)/tests && tar -xf -)

stage-mozbase: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/mozbase stage-package

stage-web-platform-tests: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/web-platform stage-package

stage-instrumentation-tests: make-stage-dir
	$(MAKE) -C $(DEPTH)/testing/instrumentation stage-package

.PHONY: \
  mochitest \
  mochitest-plain \
  mochitest-chrome \
  mochitest-devtools \
  mochitest-a11y \
  mochitest-ipcplugins \
  reftest \
  crashtest \
  xpcshell-tests \
  jstestbrowser \
  package-tests \
  make-stage-dir \
  stage-b2g \
  stage-config \
  stage-mochitest \
  stage-reftest \
  stage-xpcshell \
  stage-jstests \
  stage-android \
  stage-jetpack \
  stage-mozbase \
  stage-tps \
  stage-modules \
  stage-marionette \
  stage-steeplechase \
  stage-web-platform-tests \
  stage-instrumentation-tests \
  stage-luciddream \
  test-packages-manifest \
  test-packages-manifest-tc \
  $(NULL)

