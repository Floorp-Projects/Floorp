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

DEPTH = ../..

LIBRARY_NAME = raptorhtmlpars

DEFINES = -D_IMPL_NS_HTMLPARS

CPPSRCS=					\
	nsDTDUtils.cpp				\
	CNavDTD.cpp				\
	COtherDTD.cpp				\
	CRtfDTD.cpp				\
	nsDTDDebug.cpp				\
	nsHTMLEntities.cpp			\
	nsHTMLNullSink.cpp			\
	nsHTMLTags.cpp				\
	nsHTMLTokens.cpp			\
	nsLoggingSink.cpp			\
	nsParser.cpp				\
	CParserContext.cpp			\
	nsParserFactory.cpp			\
	nsParserNode.cpp			\
	nsScanner.cpp				\
	nsToken.cpp				\
	nsTokenHandler.cpp			\
	nsHTMLContentSinkStream.cpp		\
	nsValidDTD.cpp				\
	nsWellFormedDTD.cpp			\
	nsViewSourceHTML.cpp \
  nsXIFDTD.cpp				\
	prstrm.cpp				\
	$(NULL)

EXPORTS =     \
  nshtmlpars.h    \
  nsIContentSink.h  \
  nsIHTMLContentSink.h  \
  nsHTMLContentSinkStream.h \
  nsHTMLEntities.h    \
  nsHTMLTokens.h    \
  nsHTMLTags.h    \
  nsILoggingSink.h   \
  nsIParserNode.h   \
  nsIParser.h   \
  nsIDTD.h \
  nsIParserFilter.h \
  nsToken.h   \
  CNavDTD.h \
  COtherDTD.h \
  nsWellFormedDTD.h \
  nsValidDTD.h \
  CRtfDTD.h \
  nsXIFDTD.h \
  nsParserCIID.h \
  $(NULL)

EXTRA_EXPORTS =					\
	nsHTMLTags.h				\
	$(NULL)

MODULE = raptor

REQUIRES = xpcom netlib raptor

include $(DEPTH)/config/config.mk

include $(DEPTH)/config/rules.mk

export:: generate
	$(INSTALL) $(EXTRA_EXPORTS) $(PUBLIC)/$(MODULE)

generate: nsHTMLTags.h nsHTMLEntities.cpp

$(OBJDIR)/nsHTMLTags.o: nsHTMLTags.h

GENTAGS = $(DEPTH)/htmlparser/tools/gentags.pl
GENENTITIES = $(DEPTH)/htmlparser/tools/genentities.pl

nsHTMLTags.h: $(GENTAGS)
	rm -f nsHTMLTags.h nsHTMLTags.cpp
	$(PERL) $(GENTAGS) nsHTMLTags

nsHTMLEntities.cpp: $(GENENTITIES)
	rm -f nsHTMLEntities.cpp
	$(PERL) $(GENENTITIES) nsHTMLEntities
