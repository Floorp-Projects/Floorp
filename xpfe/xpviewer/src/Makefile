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

CPPSRCS = \
	nsBrowserMain.cpp \
	nsBrowserWindow.cpp \
	nsSetupRegistry.cpp \
	nsViewerApp.cpp \
	$(NULL)

# This file is still a Windows-only file:  -mcafee
# JSConsole.cpp

PROGRAM = xpviewer

TARGETS = $(PROGS)

INCLUDES = \
	-I$(DIST)/include \
	-I$(DEPTH)/dist/include \
	-I$(DEPTH)/include \
	-I$(PUBLIC)/dom \
	-I$(PUBLIC)/raptor \
	-I$(PUBLIC)/xpcom \
	$(NULL)

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
	$(DIST)/lib/libraptorlayout_s.a \
	$(DIST)/lib/libraptorevents_s.a \
	$(DIST)/lib/libjsdombase_s.a \
	$(DIST)/lib/libjsdomhtml_s.a \
	$(DIST)/lib/libjsdomcore_s.a \
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
	$(NULL)

# Motif, use config/mkdetect here!  Hacking Linux for now. -mcafee
LDFLAGS += \
	-lwidgetmotif \
	-lgmbasemotif \
	-L/usr/X11R6/lib -lXm -lXp -lXt -lXext -lX11 \
	$(NULL)

# Static :(
#	-lraptorwidgetmotif_s \



include $(DEPTH)/config/rules.mk


# This wasn't being used, due to the $(PROGRAM) + rules.mk stuff.
# Commenting-out for now. -mcafee
#
#$(PROGS): $(OBJS)
#	@$(MAKE_OBJDIR)
#	$(CCC) -o $@ $(LDFLAGS) $(OBJS) $(EX_LIBS) $(OS_LIBS) -lXm -lXt -lXext -lX11
