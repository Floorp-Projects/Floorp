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

nss_build_all: build_nspr all

nss_clean_all: clobber_nspr clobber

NSPR_CONFIG_STATUS = $(CORE_DEPTH)/../nspr/$(OBJDIR_NAME)/config.status
NSPR_CONFIGURE = $(CORE_DEPTH)/../nspr/configure

#
# Translate coreconf build options to NSPR configure options.
#

ifeq ($(OS_TARGET),Android)
NSPR_CONFIGURE_OPTS += --with-android-ndk=$(ANDROID_NDK) --target=arm-linux-androideabi --with-android-version=$(OS_TARGET_RELEASE)
endif
ifdef BUILD_OPT
NSPR_CONFIGURE_OPTS += --disable-debug --enable-optimize
endif
ifdef USE_X32
NSPR_CONFIGURE_OPTS += --enable-x32
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
NSPR_PREFIX = $(shell pwd)/../dist/$(OBJDIR_NAME)
else
NSPR_PREFIX = $$(topsrcdir)/../dist/$(OBJDIR_NAME)
endif

$(NSPR_CONFIG_STATUS): $(NSPR_CONFIGURE)
	mkdir -p $(CORE_DEPTH)/../nspr/$(OBJDIR_NAME)
	cd $(CORE_DEPTH)/../nspr/$(OBJDIR_NAME) ; \
	$(NSPR_COMPILERS) sh ../configure \
	$(NSPR_CONFIGURE_OPTS) \
	--with-dist-prefix='$(NSPR_PREFIX)' \
	--with-dist-includedir='$(NSPR_PREFIX)/include'

build_nspr: $(NSPR_CONFIG_STATUS)
	$(MAKE) -C $(CORE_DEPTH)/../nspr/$(OBJDIR_NAME)

clobber_nspr: $(NSPR_CONFIG_STATUS)
	$(MAKE) -C $(CORE_DEPTH)/../nspr/$(OBJDIR_NAME) clobber

build_docs:
	$(MAKE) -C $(CORE_DEPTH)/doc

clean_docs:
	$(MAKE) -C $(CORE_DEPTH)/doc clean

nss_RelEng_bld: import all

package:
	$(MAKE) -C pkg publish

