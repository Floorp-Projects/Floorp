#!gmake
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# 
#

DEPTH		= ../../..
MODULE		= rdf
LIBRARY_NAME	= $(LITE_PREFIX)rdf
LIBXP		= $(DIST)/lib/libxp.$(LIB_SUFFIX)

REQUIRES	= nspr dbm java js htmldlgs util img layer pref ldap network parse

ifdef NU_CACHE
REQUIRES 	+= cache
endif

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
		  jsec2rdf.c \
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
