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

DEPTH=../../../..

TARGET = $(LIBRARY)

LIBRARY_NAME=cachelib

MISCDEP = 		    \
    $(LIBNSPR)  	    \
    $(DIST)/lib/xpcom32.lib \
    $(NULL)

MODULE = cache

CPPSRCS	= nsCacheObject.cpp 	\
		  nsCacheModule.cpp 	\
		  nsCacheManager.cpp 	\
		  nsMemModule.cpp 		\
		  nsDiskModule.cpp 		\
		  nsCacheTrace.cpp 		\
		  nsCachePref.cpp 		\
		  nsMemCacheObject.cpp	\
	 	  $(NULL)

REQUIRES = dbm nspr pref xpcom 

LLIBS =				 \
 	$(DIST)/lib/xpcom32.lib  \
	$(LIBNSPR)		 \
	$(NULL)

include $(DEPTH)/config/rules.mk

INCLUDES += -I../include	\
			-I../public		\
			$(NULL)

