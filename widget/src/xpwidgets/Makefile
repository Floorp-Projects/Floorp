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

LIBRARY_NAME = raptorbasewidget_s

#LCFLAGS=-D_IMPL_NS_WIDGET 

ifeq ($(OS_TARGET),Rhapsody)
INCLUDES += -I$(PUBLIC)/xpcom -I$(PUBLIC)/raptor -I../rhapsody
else
INCLUDES += -I../public -I$(PUBLIC)/xpcom -I$(PUBLIC)/raptor -I../motif -I.
endif


CPPSRCS=nsBaseWidget.cpp                \
	nsMenuButton.cpp                \
	nsToolbarItemHolder.cpp         \
	nsToolbarManager.cpp            \
	nsImageButton.cpp               \
	nsToolbar.cpp                   \
	$(NULL)

MODULE=raptor 

REQUIRES=util img xpcom raptor netlib

TARGETS = $(LIBRARY)


MKSHLIB :=

include $(DEPTH)/config/rules.mk

