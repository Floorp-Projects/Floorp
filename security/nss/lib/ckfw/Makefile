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
MAKEFILE_CVS_ID = "@(#) $RCSfile: Makefile,v $ $Revision: 1.14 $ $Date: 2007/06/19 08:03:45 $"

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk
include $(CORE_DEPTH)/coreconf/rules.mk

ifdef MOZILLA_CLIENT
NSS_BUILD_CAPI = 1
endif

# This'll need some help from a build person.

# The generated files are checked in, and differ from what ckapi.perl
# will produce.  ckapi.perl is currently newer than the targets, so
# these rules are invoked, causing the wrong files to be generated.
# Turning off to fix builds.
#
# nssckepv.h: ck.api ckapi.perl
# nssckft.h: ck.api ckapi.perl
# nssckg.h: ck.api ckapi.perl
# nssck.api: ck.api ckapi.perl
# 	$(PERL) ckapi.perl ck.api

export:: private_export

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))
ifdef NSS_BUILD_CAPI
DIRS += capi
endif
endif

#ifeq ($(OS_ARCH), Darwin)
#DIRS += nssmkey
#endif
