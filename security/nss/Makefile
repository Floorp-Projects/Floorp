#! gmake
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

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

ifeq ($(OS_TARGET),WINNT)
NSPR_IMPORT_VERSION = v4.0.1
else
NSPR_IMPORT_VERSION = v4.0
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

moz_import::
ifeq ($(OS_ARCH),WINNT)
	cp $(DIST)/../include/* $(DIST)/include
ifdef BUILD_OPT
	cp $(DIST)/../WIN32_O.OBJ/lib/* $(DIST)/lib
else
	cp $(DIST)/../WIN32_D.OBJ/lib/* $(DIST)/lib
endif
	mv $(DIST)/lib/dbm32.lib $(DIST)/lib/dbm.lib
else
	$(NSINSTALL) -L ../../dist include $(DIST)
	$(NSINSTALL) -L ../../dist lib $(DIST)
	cp $(DIST)/lib/libmozdbm_s.a $(DIST)/lib/libdbm.a
endif

nsm:: all 
	cd ssm/lib/protocol; gmake
	cd ssm/server; gmake import; gmake

nsm_rebuild:: 
	cd ssm/lib/protocol; gmake
	cd ssm/ui; gmake
	cd ssm/server; gmake import; gmake

nsm_protocol: all
	cd ssm/lib/protocol; gmake

nsm_ui: all
	cd ssm/ui; gmake

nsm_gromit: nsm_protocol
	cd ssm/lib/client; gmake
	cd ssm/nav/gromit; gmake

nsm_server: nsm_protocol nsm_ui
	cd ssm/server; gmake import; gmake

coreconf_hack:
	cd ../coreconf; gmake
	gmake import

nss_RelEng_bld: coreconf_hack
#	gmake import; gmake
#	cd ns/svrcore; gmake
	gmake

nsm_RelEng_srvr_coreconf_import:
	gmake import

nsm_RelEng_srvr_bld_Win: nsm_RelEng_srvr_coreconf_import nsm_server

nsm_RelEng_srvr_bld: coreconf_hack nsm_server
