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

EXTRA_DSO_LDOPTS += -L$(DIST)/bin -lxpcom -lraptorbase -lreg $(LIBNSPR) -lplc21

CPPSRCS= \
	nsObject.cpp \
	$(NULL)

#########################################
# Here's what unix/Motif had as of 9/18:
#########################################
#       nsWidgetSupport.cpp
#	nsWindow.cpp
#	nsComboBox.cpp
#	nsTextHelper.cpp
#	nsTextAreaWidget.cpp
#	nsTextWidget.cpp
#	nsListBox.cpp
#	nsRadioButton.cpp
#	nsFileWidget.cpp
#	nsCheckButton.cpp
#	nsScrollbar.cpp
#	nsButton.cpp
#	nsAppShell.cpp
#	nsWidgetFactory.cpp
#	nsXtEventHandler.cpp
#	nsLookAndFeel.cpp
#	nsToolkit.cpp
#	nsLabel.cpp
#	nsDialog.cpp
#       nsXtManageWidget.cpp

include $(DEPTH)/config/config.mk

TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk







