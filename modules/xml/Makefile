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


DEPTH		= ../..
MODULE		= xml
LIBRARY_NAME	= $(LITE_PREFIX)xml

REQUIRES	= nspr img layer util js
EXPORTS		= expat/xmlparse/xmlparse.h 

CSRCS		= expat/xmltok/xmltok.c \
		  expat/xmltok/xmlrole.c \
		  expat/xmlparse/xmlparse.c \
		  expat/xmlparse/hashtable.c \
		  glue/xmlglue.c \
		  glue/xmldom.c \
		  glue/xmlss.c 

LOCAL_INCLUDES	= -I./expat/xmltok -I./expat/xmlparse/ -I./glue

include $(DEPTH)/config/rules.mk

$(LIBRARY): $(OBJS)
