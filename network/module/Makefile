#!gmake
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

DEPTH			= ../..

MODULE			= netlib
LIBRARY_NAME		= netlib

CPPSRCS			= \
			  nsNetService.cpp    \
			  nsNetStream.cpp     \
			  nsURL.cpp           \
			  nsHttpUrl.cpp       \
			  nsStubContext.cpp   \
			  nsNetStubs.cpp      \
			  $(NULL)

REQUIRES		= raptor js dbm nspr security pref xpcom util img \
			  layer network netcache fileurl ftpurl abouturl \
			  httpurl gophurl remoturl

EXPORTS = nsIStreamListener.h           \
          nsINetService.h               \
          nsIURL.h                      \
          nsIPostToServer.h             \
          nsIHttpUrl.h                  \
          nsINetContainerApplication.h  \
          $(NULL)

include $(DEPTH)/config/config.mk

include $(DEPTH)/config/rules.mk

$(LIBRARY): \
	$(OBJS)

