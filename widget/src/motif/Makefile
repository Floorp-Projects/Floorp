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


DEPTH = ../../..


LIBRARY_NAME = widgetunix

MODULE=raptor

REQUIRES=util img xpcom raptor netlib

DEFINES = -D_IMPL_NS_WIDGET 

#EXTRA_DSO_LDOPTS+= -lnspr21 -lplds21 -lplc21 -lreg -lraptorbase -lgfxunix -L$(DIST)/lib $(DIST)/lib/libnspr21.a $(DIST)/lib/libreg.a $(DIST)/lib/libraptorbase.a -L/usr/X11R6/lib $(DIST)/lib/libgfxunix.a -lXm -lXt -lX11
EXTRA_DSO_LDOPTS+= -lnspr21 -lplds21 -lplc21 -lreg -lraptorbase -lgfxunix -L$(DIST)/lib $(DIST)/lib/libnspr21.a $(DIST)/lib/libxpcom.a $(DIST)/lib/libreg.a $(DIST)/lib/libraptorbase.a -L/usr/X11R6/lib $(DIST)/lib/libgfxunix.a -lXm -lXt -lX11

#LLIBS+=$(DIST)/lib/libxpcom.a $(DIST)/lib/libraptorbase.a $(DIST)/lib/libgfxunix.a

CPPSRCS= \
	nsScrollbar.cpp \
	nsButton.cpp \
	nsAppShell.cpp \
	nsWidgetFactory.cpp \
	nsWindow.cpp \
	nsXtEventHandler.cpp \
	nsToolkit.cpp

CPP_OBJS=				\
 ./$(OBJDIR)/nsButton.o		\
 ./$(OBJDIR)/nsScrollbar.o		\
 ./$(OBJDIR)/nsAppShell.o		\
 ./$(OBJDIR)/nsWidgetFactory.o		\
 ./$(OBJDIR)/nsWindow.o		\
 ./$(OBJDIR)/nsToolkit.o		\
 ./$(OBJDIR)/nsXtEventHandler.o		\
 $(NULL)

include $(DEPTH)/config/config.mk

TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk







