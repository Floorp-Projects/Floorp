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

	TOOLKIT_GFX_LIB     = -lgfxgtk
	TOOLKIT_WIDGET_LIB  = -lwidgetgtk
	TOOLKIT_BASE_LIB    = -lgmbasegtk

	TOOLKIT_CFLAGS = \
		-DWIDGET_DLL=\"libwidgetgtk.so\" \
		-DGFXWIN_DLL=\"libgfxgtk.so\"

	TOOLKIT_CPPSRCS = 

	TOOLKIT_INCLUDES = $(shell gtk-config --cflags)

else

	TOOLKIT_GFX_LIB     = -lgfxmotif
	TOOLKIT_WIDGET_LIB  = -lwidgetmotif
	TOOLKIT_BASE_LIB    = -lgmbasemotif

	TOOLKIT_CFLAGS = \
		-DWIDGET_DLL=\"libwidgetmotif.so\" \
		-DGFXWIN_DLL=\"libgfxmotif.so\"

	TOOLKIT_CPPSRCS = 

endif


CPPSRCS = \
	$(TOOLKIT_CPPSRCS) \
	nsBrowserMain.cpp \
	nsBrowserWindow.cpp \
	nsSetupRegistry.cpp \
	nsViewerApp.cpp \
	$(NULL)

# This file is still a Windows-only file:  -mcafee
# JSConsole.cpp

PROGS = $(OBJDIR)/xpviewer

TARGETS = $(PROGS)

INCLUDES = \
	-I$(DIST)/include \
	-I$(DEPTH)/dist/include \
	-I$(DEPTH)/include \
	-I$(PUBLIC)/dom \
	-I$(PUBLIC)/raptor \
	-I$(PUBLIC)/xpcom \
	$(NULL)

ifdef MOZ_OJI
	JSJ_LIB = -ljsj
endif

LDFLAGS += \
	-L$(DIST)/bin \
	-lraptorbase \
	-lxpcom \
	-ljs \
	-lpref \
	-limg \
	-lpng \
	-ljpeg \
	-lraptorgfx \
	-lraptorhtmlpars \
	$(DIST)/lib/libraptorhtmlbase_s.a \
	-lraptorwebwidget \
	$(DIST)/lib/libraptorhtmlstyle_s.a \
	$(DIST)/lib/libjsdomcss_s.a \
	$(DIST)/lib/libraptorlayout_s.a \
	$(DIST)/lib/libraptorevents_s.a \
	$(DIST)/lib/libjsdombase_s.a \
	$(DIST)/lib/libjsdomhtml_s.a \
	$(DIST)/lib/libjsdomcore_s.a \
	$(JSJ_LIB) \
	-lhttpurl \
	-labouturl \
	-lmimetype \
	-lnetlib \
	-lnetwork \
	-lnetcache \
	-lnetcnvts \
	-lfileurl \
	-ljsurl \
	-lgophurl \
	-lftpurl \
	-lremoturl \
	-lnetutil \
	-lpwcac \
	-lsecfree \
	-lzlib \
	-lxp \
	-lutil \
	-lplc21 \
	-lnspr21 \
	-lplds21 \
	$(TOOLKIT_WIDGET_LIB) \
	$(TOOLKIT_BASE_LIB) \
	$(NULL)

include $(DEPTH)/config/rules.mk

# Reasonable? defaults
TOOLKIT_LIB_PATH = -L/usr/X11R6/lib
TOOLKIT_LDFLAGS = -lXm -lXt -lXext -lX11

ifeq ($(OS_ARCH),Linux)
	TOOLKIT_LIB_PATH    = -L/usr/X11R6/lib
	TOOLKIT_LDFLAGS     = -lXm -lXp -lXt -lXext -lX11 -lSM -lICE
	EXTRA_PRE_LDFLAGS   = -rdynamic
	EXTRA_POST_LDFLAGS  =
	POST_LINK_COMMAND   = true
endif

ifeq ($(OS_ARCH),IRIX)
	TOOLKIT_LIB_PATH    = 
	TOOLKIT_LDFLAGS     = -lXm -lXt -lX11
	EXTRA_PRE_LDFLAGS   = -woff 84,85
	EXTRA_POST_LDFLAGS  =
	POST_LINK_COMMAND   = true
endif

ifeq ($(OS_ARCH),SunOS)
	TOOLKIT_LIB_PATH    = -L/usr/openwin/lib -L/usr/dt/lib
	TOOLKIT_LDFLAGS     = -lXm -lXt -lXext -lX11
	EXTRA_PRE_LDFLAGS   =
	EXTRA_POST_LDFLAGS  =
	POST_LINK_COMMAND   = true
endif

ifeq ($(OS_ARCH),HP-UX)
	TOOLKIT_LIB_PATH    = -L/usr/lib/X11R5 -L/usr/lib/Motif1.2
	TOOLKIT_LDFLAGS     = -lXm -lXt -lX11
	EXTRA_PRE_LDFLAGS   =
	EXTRA_POST_LDFLAGS  = -lpthread
	POST_LINK_COMMAND   = chatr +s enable
endif


$(PROGS): $(OBJS)
	@$(MAKE_OBJDIR)
	$(CCC) $(EXTRA_PRE_LDFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) $(TOOLKIT_LIB_PATH) $(TOOLKIT_LDFLAGS) $(EXTRA_POST_LDFLAGS)
	$(POST_LINK_COMMAND) $@

export::

install:: $(TARGETS)
	$(INSTALL) $(PROGS) $(DIST)/bin
