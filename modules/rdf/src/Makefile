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
LIBXP		= $(DIST)/lib/libxp.$(LIB_SUFFIX)

REQUIRES	= nspr dbm java js htmldlgs util img layer pref ldap

JNI_GEN		= netscape.rdf.core.NativeRDF netscape.rdf.core.NativeRDFEnumeration

CSRCS		= \
		  vocab.c \
		  mcf.c \
		  remstore.c \
		  utils.c \
		  rdfparse.c \
		  mcff2mcf.c \
		  bmk2mcf.c \
		  ldap2rdf.c \
		  glue.c \
		  rdfht.c \
		  $(NULL)

ifndef RDF_STANDALONE
CSRCS		+= \
		  nlcstore.c \
		  find2rdf.c \
		  fs2rdf.c \
		  hist2rdf.c \
		  pm2rdf.c \
		  es2mcf.c \
		  columns.c \
		  rdfjava.c \
		  ht.c \
		  $(NULL)
endif

SPF_OBJS	= $(OBJDIR)/spf2ldif.o \
		  $(OBJDIR)/hashtable.o

LOCAL_INCLUDES	= -I$(PUBLIC)/rdf

include $(DEPTH)/config/rules.mk

ifdef RDF_STANDALONE
DEFINES := $(filter-out -DMOZILLA_CLIENT, $(DEFINES))
endif

$(LIBRARY): $(OBJS)

test:		$(OBJDIR)/test
spf2ldif:	$(OBJDIR)/spf2ldif

.PHONY: test spf2ldif

$(OBJDIR)/test: $(OBJDIR)/test.o $(LIBRARY)
	    @$(MAKE_OBJDIR)
	    $(CC) -o $@ $(OBJDIR)/test.o $(LIBRARY) $(LIBNSPR) $(LIBXP) $(LDFLAGS) $(OS_LIBS) 
	    
$(OBJDIR)/spf2ldif: $(SPF_OBJS)  $(LIBRARY)
	    @$(MAKE_OBJDIR)
	    $(CC) -o $@ $(SPF_OBJS) $(LIBRARY) $(LIBNSPR) $(LIBXP) $(LDFLAGS) $(OS_LIBS) 
