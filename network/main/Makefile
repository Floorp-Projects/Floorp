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

MODULE		= network
LIBRARY_NAME	= network

CPPSRCS= mkabook.cpp	

CSRCS		= \
	jscookie.c	\
	mkautocf.c	\
	mkconect.c	\
	mkformat.c	\
	mkfsort.c	\
	mkgeturl.c	\
	mkhelp.c	\
	mkinit.c	\
	mkmessag.c	\
	mkpadpac.c	\
	mkparse.c	\
	mkselect.c	\
	mksockrw.c	\
	mksort.c	\
	mkstream.c	\
	mktrace.c	\
	mkutils.c	\
	prefetch.c	\
	unix-dns.c	\
	$(NULL)

EXPORTS= mkstream.h mkformat.h mkparse.h mkfsort.h mksort.h mkgeturl.h \
	mkselect.h mktcp.h netutils.h mkpadpac.h mkautocf.h mkhelp.h


REQUIRES	= jtools nspr2 dbm util js parse lay style pref htmldlgs java \
	libfont netcache httpurl cnetinit security img layer netcnvts network

include $(DEPTH)/config/rules.mk

TEST_LIBS	= $(LIBRARY) \
		  $(DIST)/lib/liburl.$(LIB_SUFFIX) \
		  $(DIST)/lib/libsec-us.$(LIB_SUFFIX) \
		  $(DIST)/lib/libdbm.$(LIB_SUFFIX) \
		  $(DIST)/lib/libxp.$(LIB_SUFFIX) \
		  $(DIST)/lib/nspr.$(LIB_SUFFIX)

test:
	$(CC) -g -o $(OBJDIR)/style_test $(CFLAGS) -DSS_TEST stystruc.c $(TEST_LIBS)
	$(CC) -g -o $(OBJDIR)/stack_test $(CFLAGS) -DTEST_STYLESTACK stystack.c $(TEST_LIBS)

