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

include $(DEPTH)/config/config.mk

LIBRARY_NAME = widgetunix

MODULE=raptor

REQUIRES=util img xpcom raptor netlib

DEFINES = -D_IMPL_NS_WIDGET 

EXTRA_DSO_LDOPTS+=\
	$(DIST)/bin/libxpcom.so \
	$(DIST)/bin/libraptorbase.so \
	$(DIST)/bin/libreg.so \
	$(LIBNSPR)\
	$(DIST)/lib/libplc21.a

# This should really be done properly in 'config'
ifeq ($(OS_ARCH),Linux)
EXTRA_DSO_LDOPTS+=\
	-L/usr/X11R6/lib -lX11 -lm
else
EXTRA_DSO_LDOPTS+= -lXm -lXt -lX11 -lm 
endif

CPPSRCS= \
        nsMenuBar.cpp \
        nsMenu.cpp \
        nsMenuItem.cpp \
        nsPopUpMenu.cpp \
        nsWidgetSupport.cpp \
	nsComboBox.cpp \
	nsTextHelper.cpp \
	nsTextAreaWidget.cpp \
	nsTextWidget.cpp \
	nsListBox.cpp \
	nsRadioButton.cpp \
	nsFileWidget.cpp \
	nsCheckButton.cpp \
	nsScrollbar.cpp \
	nsButton.cpp \
	nsAppShell.cpp \
	nsLookAndFeel.cpp \
	nsToolkit.cpp \
	nsLabel.cpp \
	nsDialog.cpp \
	nsWidgetFactory.cpp \
	nsWindow.cpp \
	nsXtEventHandler.cpp \
        nsXtManageWidget.cpp

CPP_OBJS=				\
 ./$(OBJDIR)/nsMenuButton.o  \
 ./$(OBJDIR)/nsPopUpMenu.o  \
 ./$(OBJDIR)/nsToolbarItemHolder.o  \
 ./$(OBJDIR)/nsToolbarManager.o  \
 ./$(OBJDIR)/nsImageButton.o  \
 ./$(OBJDIR)/nsToolBar.obj  \
 ./$(OBJDIR)/nsWidgetSupport.o		\
 ./$(OBJDIR)/nsComboBox.o		\
 ./$(OBJDIR)/nsFileWidget.o		\
 ./$(OBJDIR)/nsTextHelper.o		\
 ./$(OBJDIR)/nsTextAreaWidget.o		\
 ./$(OBJDIR)/nsTextWidget.o		\
 ./$(OBJDIR)/nsListBox.o		\
 ./$(OBJDIR)/nsRadioButton.o		\
 ./$(OBJDIR)/nsFileWidget.o		\
 ./$(OBJDIR)/nsCheckButton.o		\
 ./$(OBJDIR)/nsScrollbar.o		\
 ./$(OBJDIR)/nsButton.o		        \
 ./$(OBJDIR)/nsScrollbar.o		\
 ./$(OBJDIR)/nsAppShell.o		\
 ./$(OBJDIR)/nsLookAndFeel.o		\
 ./$(OBJDIR)/nsToolkit.o		\
 ./$(OBJDIR)/nsLabel.o			\
 ./$(OBJDIR)/nsDialog.o		\
 ./$(OBJDIR)/nsWidgetFactory.o		\
 ./$(OBJDIR)/nsWindow.o		\
 ./$(OBJDIR)/nsXtEventHandler.o		\
 $(NULL)

include $(DEPTH)/config/config.mk

TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk







