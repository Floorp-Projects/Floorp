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

LIBRARY_NAME = raptorwebwidget

MODULE=raptor

REQUIRES=xpcom raptor dom js netlib

INCLUDES += -I../public -I$(PUBLIC)/raptor -I$(PUBLIC)/xpcom -I$(PUBLIC)/dom -I$(PUBLIC)/js -I(PUBLIC)/netlib

DEFINES = -D_IMPL_NS_WEB

CPPSRCS=				\
 nsWebWidget.cpp			\
 nsLinkHandler.cpp			\
 $(NULL)

CPP_OBJS=				\
 ./$(OBJDIR)/nsWebWidget.o		\
 ./$(OBJDIR)/nsLinkHandler.o		\
 $(NULL)

include $(DEPTH)/config/config.mk

TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk

