#! gmake
#
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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

ifeq ($(OS_TARGET),WINCE)
DIRS	= lib	# omit cmd since wince has no command line shell
endif

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
	cd $(CORE_DEPTH)/coreconf ;  $(MAKE)

clobber_coreconf:
	cd $(CORE_DEPTH)/coreconf ;  $(MAKE) clobber

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
	cd $(CORE_DEPTH)/../nsprpub/$(OBJDIR_NAME) ; $(MAKE)

clobber_nspr: $(NSPR_CONFIG_STATUS)
	cd $(CORE_DEPTH)/../nsprpub/$(OBJDIR_NAME) ; $(MAKE) clobber

build_dbm:
ifdef NSS_DISABLE_DBM
	@echo "skipping the build of DBM"
else
	cd $(CORE_DEPTH)/dbm ; $(MAKE) export libs
endif

clobber_dbm:
	cd $(CORE_DEPTH)/dbm ; $(MAKE) clobber

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
