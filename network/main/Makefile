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

include $(DEPTH)/config/config.mk

MODULE		= network
LIBRARY_NAME	= network

ifdef MOZ_MAIL_NEWS
CPPSRCS= mkabook.cpp	
endif

CSRCS		= \
	jscookie.c	\
	mkautocf.c	\
	mkconect.c	\
	mkfsort.c	\
	mkgeturl.c	\
	mkhelp.c	\
	mkmessag.c	\
	mkpadpac.c	\
	mkparse.c	\
	mkselect.c	\
	mksockrw.c	\
	mksort.c	\
	mktrace.c	\
	mkutils.c	\
	prefetch.c	\
	unix-dns.c	\
	$(NULL)

ifndef MODULAR_NETLIB
CSRCS += \
	mkinit.c	\
	mkstream.c	\
	$(NULL)
endif

EXPORTS= mkstream.h mkparse.h mkfsort.h mksort.h mkgeturl.h \
	netstream.h mkselect.h mktcp.h netutils.h mkpadpac.h mkautocf.h mkhelp.h \
        mkutils.h mktrace.h

ifdef MOZ_MAIL_NEWS
EXPORTS += mkldap.h
endif # MOZ_MAIL_NEWS


REQUIRES	= ldapurl imap4url jtools progress nspr2 dbm util js parse lay style pref htmldlgs java \
	libfont netcache httpurl cnetinit security img layer netcnvts network \
	mimetype ldap net

ifdef MOZ_MAIL_NEWS
REQUIRES += mailbxurl nntpurl pop3url
endif

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

