# -*- Mode: Makefile -*-
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



DEPTH			= ../../..

MODULE			= security
LIBRARY_NAME		= secfree

EXPORTS			= base64.h \
			  cert.h \
			  preenc.h \
			  seccomon.h \
			  secerr.h \
			  sechash.h \
			  secnav.h \
			  secrng.h \
			  secstubn.h \
			  secstubs.h \
			  secstubt.h \
			  ssl.h \
			  sslerr.h \
			  zig.h \
			  rosetta.h \
			  $(NULL)

CSRCS			= secport.c \
			  navstubs.c \
			  $(NULL)

REQUIRES		= dbm js img layer util

include $(DEPTH)/config/rules.mk

$(LIBRARY): $(OBJS)
