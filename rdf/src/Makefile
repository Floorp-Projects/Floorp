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

DEPTH		= ../..
MODULE		= rdf
LIBRARY_NAME	= rdf


REQUIRES	= nspr xpcom netlib raptor

LOCAL_INCLUDES=			\
	-I$(PUBLIC)/rdf		\
	-I$(PUBLIC)/xpcom	\
	-I$(PUBLIC)/netlib	\
	$(NULL)

CSRCS=			\
	vocab.c 	\
	core.c		\
	remstore.c 	\
	utils.c 	\
	rdfparse.c 	\
	bmk2rdf.c 	\
	columns.c 	\
	rdfht.c 	\
	$(NULL)

CPPSRCS=				\
	comwrap.cpp			\
	netglue.cpp			\
	nsRDFDataModel.cpp		\
	nsRDFDataModelItem.cpp		\
	nsRDFToolbarDataModel.cpp	\
	nsRDFToolbarDataModelItem.cpp	\
	nsRDFTreeColumn.cpp		\
	nsRDFTreeDataModel.cpp		\
	nsRDFTreeDataModelItem.cpp	\
	$(NULL)

include $(DEPTH)/config/config.mk

TARGET = $(LIBRARY)

include $(DEPTH)/config/rules.mk

