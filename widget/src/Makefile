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

DEPTH=../..

ifeq ($(OS_TARGET),Rhapsody)
DIRS = rhapsody
else
DIRS = motif
endif

include $(DEPTH)/config/config.mk

TARGET = $(LIBRARY)

include $(DEPTH)/config/rules.mk
--------------------------------------
DEPTH=..\..
#IGNORE_MANIFEST=1

ifeq ($(OS_TARGET),Rhapsody)
DIRS = rhapsody
else
DIRS = motif
endif

LIBRARY_NAME=raptorbasewidget_s
MODULE=raptor 
REQUIRES=xpcom 

DEFINES= -DWIN32_LEAN_AND_MEAN -D_IMPL_NS_WIDGET

CPPSRCS= \
	nsBaseWidget.cpp 

EXPORTS=nsBaseWidget.h 

CPP_OBJS=.\$(OBJDIR)\nsBaseWidget.obj  \
	.\$(OBJDIR)\nsMenuButton.obj  \
	.\$(OBJDIR)\nsToolbarItemHolder.obj  \
	.\$(OBJDIR)\nsToolbarManager.obj  \
	.\$(OBJDIR)\nsImageButton.obj  \
	.\$(OBJDIR)\nsToolBar.obj  
    

LINCS= -I$(PUBLIC)\xpcom -I$(PUBLIC)\raptor -Iwindows 

LCFLAGS = \
	$(LCFLAGS) \
	$(DEFINES) \
	$(NULL)

include <$(DEPTH)\config\rules.mak>

libs:: $(LIBRARY)
	$(MAKE_INSTALL) $(LIBRARY) $(DIST)\lib

	
clobber::
	rm -f $(DIST)\lib\$(LIBRARY_NAME).lib
