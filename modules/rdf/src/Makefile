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

#
# 
#

DEPTH		= ../../..
MODULE		= rdf
LIBRARY_NAME	= $(LITE_PREFIX)rdf

REQUIRES	= nspr dbm java js htmldlgs util img layer pref

JNI_GEN		= netscape.rdf.core.NativeRDF netscape.rdf.core.NativeRDFEnumeration

CSRCS		= bmk2mcf.c \
		  vocab.c \
		  nlcstore.c \
		  mcff2mcf.c \
		  rdfht.c \
		  remstore.c \
		  glue.c \
		  utils.c \
		  fs2rdf.c \
		  hist2rdf.c \
		  rdfparse.c \
		  pm2rdf.c \
		  es2mcf.c \
		  columns.c \
		  xmlparse.c \
		  xmlglue.c \
		  xmlss.c \
		  scook.c \
		  rdfjava.c \
		  mcf.c \
		  ht.c \
		  $(NULL)

LOCAL_INCLUDES	= -I$(PUBLIC)/rdf

include $(DEPTH)/config/rules.mk

$(LIBRARY): $(OBJS)

test:	$(OBJDIR)/test

.PHONY: test

$(OBJDIR)/test: $(OBJDIR)/test.o $(LIBRARY)
	    @$(MAKE_OBJDIR)
	    $(CC) -o $@ $(OBJDIR)/test.o $(LIBRARY) $(LIBNSPR) $(DIST)/lib/libdbm.$(LIB_SUFFIX) $(LDFLAGS)
