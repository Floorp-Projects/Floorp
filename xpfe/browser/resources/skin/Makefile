#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
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

DEPTH		= ../../../..
topsrcdir	= /export/waterson/seamonkey-merge/mozilla
VPATH		= /export/waterson/seamonkey-merge/mozilla/xpfe/browser/resources/skin
srcdir		= /export/waterson/seamonkey-merge/mozilla/xpfe/browser/resources/skin

include $(DEPTH)/config/autoconf.mk
include $(topsrcdir)/config/rules.mk

EXPORT_RESOURCE_CONTENT = \
	$(srcdir)/alertl.gif \
	$(srcdir)/errorl.gif \
	$(srcdir)/messagel.gif \
	$(srcdir)/navigator.css \
	$(srcdir)/questionl.gif \
	$(srcdir)/contentframe.css \
	$(srcdir)/animthrob.gif \
	$(srcdir)/animthrob_single.gif \
	$(NULL)

install:: 
	$(INSTALL) $(EXPORT_RESOURCE_CONTENT) $(DIST)/bin/chrome/navigator/skin/default

