#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
# 
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

#! gmake

MOD_DEPTH = ../../..

include $(MOD_DEPTH)/config/config.mk

RELEASE_HEADERS = pprio.h pprthred.h prpriv.h
RELEASE_HEADERS_DEST = $(RELEASE_INCLUDE_DIR)/private

HEADERS = $(RELEASE_HEADERS) pprmwait.h primpl.h

include $(MOD_DEPTH)/config/rules.mk

export:: $(HEADERS)
	$(INSTALL) -m 444 $(HEADERS) $(DIST)/include/private
ifeq ($(MOZ_BITS),16)
	$(INSTALL) -m 444 $(HEADERS) $(MOZ_INCL)/private
endif
