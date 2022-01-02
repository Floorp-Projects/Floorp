#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################



#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################



#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################



#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

TESTPACKAGE="nss-$(OS_TARGET)$(CPU_TAG).tgz"
RTSH=$(DIST)/../../runtests.sh
PCFG=$(DIST)/platform.cfg


#Hint: In order to test the Makefiles without running the tests, use:
#      make NSS_CYCLES="standard" NSS_TESTS="dummy"

ifeq ($(OS_TARGET),Android)
TEST_SHELL?=$$HOME/bin/sh
ANDROID_PORT?="2222"
#Define the subset of tests that is known to work on Android
NSS_CYCLES?="standard pkix sharedb"
NSS_TESTS?="cipher lowhash libpkix cert dbtests tools sdr crmf smime ssl ocsp merge pkits chains"
NSS_SSL_TESTS?="crl normal_normal iopr"
NSS_SSL_RUN?="cov auth stress"
else
TEST_SHELL?="/bin/sh"
endif

# Create a package for test execution on a separate system.
package_for_testing:
	echo "export OBJDIR=$(OBJDIR_NAME)"     > $(PCFG)
	echo "export OS_ARCH=$(OS_ARCH)"       >> $(PCFG)
	echo "export OS_TARGET=$(OS_TARGET)"   >> $(PCFG)
	echo "export DLL_PREFIX=$(DLL_PREFIX)" >> $(PCFG)
	echo "export DLL_SUFFIX=$(DLL_SUFFIX)" >> $(PCFG)
	echo 'echo "set HOST and DOMSUF if your system is not registered in DNS"' > $(RTSH)
	cat $(PCFG)                                  >> $(RTSH)
	echo 'export NSS_TESTS=$(NSS_TESTS)'         >> $(RTSH)
	echo 'export NSS_SSL_TESTS=$(NSS_SSL_TESTS)' >> $(RTSH)
	echo 'export NSS_SSL_RUN=$(NSS_SSL_RUN)'     >> $(RTSH)
	echo 'export NSS_CYCLES=$(NSS_CYCLES)'       >> $(RTSH)
	echo 'export USE_64=$(USE_64)'               >> $(RTSH)
	echo 'export BUILD_OPT=$(BUILD_OPT)'         >> $(RTSH)
	echo 'export PKITS_DATA=$(PKITS_DATA)'       >> $(RTSH)
	echo 'export NSPR_LOG_MODULES=$(NSPR_LOG_MODULES)'                   >> $(RTSH)
ifeq ($(OS_TARGET),Android)
	# Android doesn't support FIPS tests, because
	# dladdr does not return a full path for implicitly loaded libraries
	echo "export NSS_TEST_DISABLE_FIPS=1" >> $(DIST)/platform.cfg
endif
ifeq ($(CROSS_COMPILE),1)
# execute signing on test system
	echo 'export DIST=$${HOME}/nsstest/dist/'           >> $(RTSH)
	echo 'export NSPR_LIB_DIR=$${DIST}/$${OBJDIR}/lib/' >> $(RTSH)
	echo 'echo "signing"'                               >> $(RTSH)
# work around a bug in Android ash that has a corrupted work directory after login
	echo 'cd $${HOME}/nsstest'                          >> $(RTSH)
	echo 'cd nss/cmd/shlibsign'                         >> $(RTSH)
	echo '$(TEST_SHELL) ./sign.sh $${DIST}/$${OBJDIR}/ $${DIST}/$${OBJDIR}/bin $${OS_TARGET} $${NSPR_LIB_DIR} $${NSPR_LIB_DIR}$${DLL_PREFIX}freebl3.$${DLL_SUFFIX}'  >> $(RTSH)
	echo '$(TEST_SHELL) ./sign.sh $${DIST}/$${OBJDIR}/ $${DIST}/$${OBJDIR}/bin $${OS_TARGET} $${NSPR_LIB_DIR} $${NSPR_LIB_DIR}$${DLL_PREFIX}softokn3.$${DLL_SUFFIX}' >> $(RTSH)
	echo '$(TEST_SHELL) ./sign.sh $${DIST}/$${OBJDIR}/ $${DIST}/$${OBJDIR}/bin $${OS_TARGET} $${NSPR_LIB_DIR} $${NSPR_LIB_DIR}$${DLL_PREFIX}nssdbm3.$${DLL_SUFFIX}'  >> $(RTSH)
ifneq ($(OS_TARGET),Android)
# Android's ash doesn't support "export -n" yet
	echo 'export -n DIST'                          >> $(RTSH)
	echo 'export -n NSPR_LIB_DIR'                  >> $(RTSH)
endif
	echo 'cd ../../../'                            >> $(RTSH)
endif
	echo 'rm -rf tests_results'                    >> $(RTSH)
	echo 'echo "running tests"'                    >> $(RTSH)
	echo 'cd nss/tests'                            >> $(RTSH)
	# We require progress indication on stdout while running the tests (to avoid timeouts).
	set -o pipefail
	echo '$(TEST_SHELL) ./all.sh | tee ../../logfile 2>&1 |grep ": #"'          >> $(RTSH)
	RETVAL=$?
	echo 'cd ../../'                               >> $(RTSH)
	# dump test summary from end of logfile
	echo 'echo "=========="; tail -100 logfile'    >> $(RTSH)
	echo 'tar czf tests_results.tgz tests_results'                              >> $(RTSH)
	echo 'echo "created tests_results.tgz"'                                     >> $(RTSH)
	echo 'echo "results are in directory: "`ls -1d tests_results/security/*.1`' >> $(RTSH)
	echo 'echo exit status: $${RETVAL}'                                         >> $(RTSH)
	echo 'exit $${RETVAL}'                                                      >> $(RTSH)
	rm -f $(TESTPACKAGE)
	(cd $(DIST)/../.. ; tar czhf dist/$(TESTPACKAGE) runtests.sh dist/$(OBJDIR_NAME) dist/public nss/tests nss/cmd/bltest/tests nss/cmd/pk11gcmtest/tests nss/cmd/shlibsign; echo "created "`pwd`"/dist/$(TESTPACKAGE)" )

android_run_tests:
	ssh -p $(ANDROID_PORT) -o CheckHostIP=no $(ANDROID_ADDR)  'pwd; cd; pwd; cd nsstest; export PATH=$$HOME/bin:$$PATH ; $(TEST_SHELL) runtests.sh'

android_install:
	rm -f $(DIST)/android.sftp
	echo '-mkdir nsstest' > $(DIST)/android.sftp
	echo '-rm nsstest/$(TESTPACKAGE)' >> $(DIST)/android.sftp
	echo 'progress' >> $(DIST)/android.sftp
	echo 'put $(DIST)/../$(TESTPACKAGE) nsstest' >> $(DIST)/android.sftp
	sftp -o Port=$(ANDROID_PORT) -o CheckHostIP=no -b $(DIST)/android.sftp $(ANDROID_ADDR)
	ssh -p $(ANDROID_PORT) -o CheckHostIP=no $(ANDROID_ADDR)  'cd nsstest ; $$HOME/bin/rm -rf logfile runtests.sh dist security tests_results tests_results.tgz;  $$HOME/bin/tar xzf $(TESTPACKAGE)'

WORKDIR="$(DIST)/../../"
RESULTSPACKAGE=tests_results.tgz
android_get_result:
	rm -f $(WORKDIR)/result.sftp $(WORKDIR)/$(RESULTSPACKAGE)
	echo "progress" > $(WORKDIR)/result.sftp
	echo 'get nsstest/$(RESULTSPACKAGE) $(WORKDIR)' >> $(WORKDIR)/result.sftp
	sftp -o Port=$(ANDROID_PORT) -o CheckHostIP=no -b $(WORKDIR)/result.sftp $(ANDROID_ADDR) 
	(cd $(WORKDIR); tar xzf $(RESULTSPACKAGE); rm -f result.sftp $(RESULTSPACKAGE) )

# Android testing assumes having built with: OS_TARGET=Android CROSS_COMPILE=1
# Connectivity tested with Android app: SSHDroid
# Provide appropriate ANDROID_ADDR variable, e.g.:
#   make test_android ANDROID_ADDR=root@192.168.4.5
# See also: https://wiki.mozilla.org/NSS:Android

test_android: package_for_testing android_install android_run_tests android_get_result
