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

ifeq ($(USE_NGLAYOUT_GTK),1)
LIBRARY_NAME = widgetgtk
else
LIBRARY_NAME = widgetmotif
endif

MODULE=raptor 

REQUIRES=util img xpcom raptor netlib

INCLUDES+= -I../xpwidgets -I../motif -I.

CPPSRCS=nsWidgetSupport.cpp nsMotifWidgetFactory.cpp

ifeq ($(USE_NGLAYOUT_GTK),1)
TOOLKIT_DSO_LDOPTS = $(DIST)/lib/libraptorwidgetgtk_s.a
else
TOOLKIT_DSO_LDOPTS = $(DIST)/lib/libraptorwidgetmotif_s.a
endif

EXTRA_DSO_LDOPTS+=\
	$(TOOLKIT_DSO_LDOPTS) \
	$(DIST)/lib/libraptorbasewidget_s.a \
	-L$(DIST)/bin \
	-lxpcom \
	-lraptorbase \
	-lreg \
	$(LIBNSPR)\
	$(DIST)/lib/libplc21.a

# This should really be done properly in 'config'
ifeq ($(OS_ARCH),Linux)
EXTRA_DSO_LDOPTS+=\
	-L/usr/X11R6/lib -lX11 -lm
else
EXTRA_DSO_LDOPTS+= -lXm -lXt -lX11 -lm
endif

include $(DEPTH)/config/rules.mk

