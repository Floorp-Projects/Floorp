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

nss_build_all: build_coreconf build_nspr build_dbm all

nss_clean_all: clobber_coreconf clobber_nspr clobber_dbm clobber

build_coreconf:
	$(MAKE) -C $(CORE_DEPTH)/coreconf

clobber_coreconf:
	$(MAKE) -C $(CORE_DEPTH)/coreconf clobber

NSPR_CONFIG_STATUS = $(CORE_DEPTH)/../nsprpub/$(OBJDIR_NAME)/config.status
NSPR_CONFIGURE = $(CORE_DEPTH)/../nsprpub/configure

#
# Translate coreconf build options to NSPR configure options.
#

ifdef BUILD_OPT
NSPR_CONFIGURE_OPTS += --disable-debug --enable-optimize
endif
ifdef USE_64
NSPR_CONFIGURE_OPTS += --enable-64bit
endif
ifeq ($(OS_TARGET),WIN95)
NSPR_CONFIGURE_OPTS += --enable-win32-target=WIN95
endif
ifdef USE_DEBUG_RTL
NSPR_CONFIGURE_OPTS += --enable-debug-rtl
endif
ifdef NS_USE_GCC
NSPR_COMPILERS = CC=gcc CXX=g++
endif

#
# Some pwd commands on Windows (for example, the pwd
# command in Cygwin) return a pathname that begins
# with a (forward) slash.  When such a pathname is
# passed to Windows build tools (for example, cl), it
# is mistaken as a command-line option.  If that is the case,
# we use a relative pathname as NSPR's prefix on Windows.
#

USEABSPATH="YES"
ifeq (,$(filter-out WIN%,$(OS_TARGET)))
ifeq (,$(findstring :,$(shell pwd)))
USEABSPATH="NO"
endif
endif
ifeq ($(USEABSPATH),"YES")
NSPR_PREFIX = $(shell pwd)/../../dist/$(OBJDIR_NAME)
else
NSPR_PREFIX = $$(topsrcdir)/../dist/$(OBJDIR_NAME)
endif

$(NSPR_CONFIG_STATUS): $(NSPR_CONFIGURE)
	$(NSINSTALL) -D $(CORE_DEPTH)/../nsprpub/$(OBJDIR_NAME)
	cd $(CORE_DEPTH)/../nsprpub/$(OBJDIR_NAME) ; \
	$(NSPR_COMPILERS) sh ../configure \
	$(NSPR_CONFIGURE_OPTS) \
	--with-dist-prefix='$(NSPR_PREFIX)' \
	--with-dist-includedir='$(NSPR_PREFIX)/include'

build_nspr: $(NSPR_CONFIG_STATUS)
	$(MAKE) -C $(CORE_DEPTH)/../nsprpub/$(OBJDIR_NAME)

clobber_nspr: $(NSPR_CONFIG_STATUS)
	$(MAKE) -C $(CORE_DEPTH)/../nsprpub/$(OBJDIR_NAME) clobber

build_dbm:
ifdef NSS_DISABLE_DBM
	@echo "skipping the build of DBM"
else
	$(MAKE) -C $(CORE_DEPTH)/dbm export libs
endif

clobber_dbm:
	$(MAKE) -C $(CORE_DEPTH)/dbm clobber

moz_import::
ifeq (,$(filter-out WIN%,$(OS_TARGET)))
	$(NSINSTALL) -D $(DIST)/include/nspr
	cp $(DIST)/../include/nspr/*.h $(DIST)/include/nspr
	cp $(DIST)/../include/* $(DIST)/include
ifdef BUILD_OPT
	cp $(DIST)/../WIN32_O.OBJ/lib/* $(DIST)/lib
else
	cp $(DIST)/../WIN32_D.OBJ/lib/* $(DIST)/lib
endif
	mv $(DIST)/lib/dbm32.lib $(DIST)/lib/dbm.lib
else
ifeq ($(OS_TARGET),OS2)
	cp -rf $(DIST)/../include $(DIST)
	cp -rf $(DIST)/../lib $(DIST)
	cp -f $(DIST)/lib/libmozdbm_s.$(LIB_SUFFIX) $(DIST)/lib/libdbm.$(LIB_SUFFIX)
else
	$(NSINSTALL) -L ../../dist include $(DIST)
	$(NSINSTALL) -L ../../dist lib $(DIST)
	cp $(DIST)/lib/libmozdbm_s.$(LIB_SUFFIX) $(DIST)/lib/libdbm.$(LIB_SUFFIX)
endif
endif

nss_RelEng_bld: build_coreconf import build_dbm all

package:
	$(MAKE) -C pkg publish

TESTPACKAGE="nss-$(OS_TARGET)$(CPU_TAG).tgz"
package_for_testing:
	echo "export OBJDIR=$(OBJDIR_NAME)"     > $(DIST)/platform.cfg
	echo "export OS_ARCH=$(ANDROID)"       >> $(DIST)/platform.cfg
	echo "export DLL_PREFIX=$(DLL_PREFIX)" >> $(DIST)/platform.cfg
	echo "export DLL_SUFFIX=$(DLL_SUFFIX)" >> $(DIST)/platform.cfg
ifeq ($(OS_TARGET),Android)
	# Android doesn't support FIPS tests, so don't run them
	echo "export NSS_TEST_DISABLE_FIPS=1"  >> $(DIST)/platform.cfg
endif
	echo 'echo "set HOST and DOMSUF if your system is not registered in DNS"; sleep 5' > $(DIST)/../../runtests.sh
	echo 'export NSS_TESTS=$(NSS_TESTS)'         >> $(DIST)/../../runtests.sh
	echo 'export NSS_SSL_TESTS=$(NSS_SSL_TESTS)' >> $(DIST)/../../runtests.sh
	echo 'export NSS_SSL_RUN=$(NSS_SSL_RUN)'     >> $(DIST)/../../runtests.sh
	echo 'export NSS_CYCLES=$(NSS_CYCLES)'       >> $(DIST)/../../runtests.sh
	echo 'export OBJDIR=$(OBJDIR_NAME)'          >> $(DIST)/../../runtests.sh
	echo 'export USE_64=$(USE_64)'               >> $(DIST)/../../runtests.sh
	echo 'export BUILD_OPT=$(BUILD_OPT)'         >> $(DIST)/../../runtests.sh
	echo 'rm -rf test_results'                   >> $(DIST)/../../runtests.sh
	echo 'echo "running tests"'                  >> $(DIST)/../../runtests.sh
	echo 'cd security/nss/tests; ./all.sh > ../../../logfile 2>&1 ; cd ../../../' >> $(DIST)/../../runtests.sh
	echo 'tar czf tests_results.tgz tests_results' >> $(DIST)/../../runtests.sh
	echo 'echo "created tests_results.tgz"' >> $(DIST)/../../runtests.sh
	echo 'echo "results are in directory: "`ls -1d tests_results/security/*.1`' >> $(DIST)/../../runtests.sh
	echo 'echo -n "number of PASSED tests: "; grep -cw PASSED logfile;'  >> $(DIST)/../../runtests.sh
	echo 'echo -n "number of FAILED tests: "; grep -cw FAILED logfile;'  >> $(DIST)/../../runtests.sh
	rm -f $(TESTPACKAGE)
	(cd $(DIST)/../.. ; tar czhf dist/$(TESTPACKAGE) runtests.sh dist/$(OBJDIR_NAME) dist/public security/nss/tests security/nss/cmd/bltest/tests; echo "created "`pwd`"/dist/$(TESTPACKAGE)")
