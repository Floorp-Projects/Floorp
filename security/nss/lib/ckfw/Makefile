# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
MAKEFILE_CVS_ID = "@(#) $RCSfile$ $Revision$ $Date$"

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk
include $(CORE_DEPTH)/coreconf/rules.mk

ifdef NOTDEF # was ifdef MOZILLA_CLIENT
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
ifeq (,$(filter-out WINNT WIN95,$(OS_TARGET)))
ifdef NSS_BUILD_CAPI
DIRS += capi
endif
endif

#ifeq ($(OS_ARCH), Darwin)
#DIRS += nssmkey
#endif
