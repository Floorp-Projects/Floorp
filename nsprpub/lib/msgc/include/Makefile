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

EXPORT_HEADERS = prgc.h
HEADERS = $(EXPORT_HEADERS) gcint.h

RELEASE_HEADERS = $(EXPORT_HEADERS)
RELEASE_HEADERS_DEST = $(RELEASE_INCLUDE_DIR)

include $(MOD_DEPTH)/config/rules.mk

export:: $(EXPORT_HEADERS)
	$(INSTALL) -m 444 $(EXPORT_HEADERS) $(DIST)/include
ifeq ($(MOZ_BITS),16)
	$(INSTALL) -m 444 $(EXPORT_HEADERS) $(MOZ_INCL)
endif

install:: export
