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

DEPTH=../../..

include $(DEPTH)/config/config.mk

ifeq ($(USE_NGLAYOUT_GTK),1)

	TOOLKIT_GFX_LIB     = -lgfxgtk
	TOOLKIT_WIDGET_LIB  = -lwidgetgtk
	TOOLKIT_BASE_LIB    = -lgmbasegtk

	TOOLKIT_CFLAGS = \
		-DWIDGET_DLL=\"libwidgetgtk.so\" \
		-DGFXWIN_DLL=\"libgfxgtk.so\"

	TOOLKIT_CPPSRCS = \
		nsGTKMain.cpp \
		nsGTKMenu.cpp

	TOOLKIT_INCLUDES = $(shell gtk-config --cflags)

else

	TOOLKIT_GFX_LIB        = -lgfxmotif
	TOOLKIT_WIDGET_LIB     = -lwidgetmotif
	TOOLKIT_BASE_LIB = -lgmbasemotif

	TOOLKIT_CFLAGS = \
		-DWIDGET_DLL=\"libwidgetmotif.so\" \
		-DGFXWIN_DLL=\"libgfxmotif.so\"

	TOOLKIT_CPPSRCS = \
			nsMotifMenu.cpp \
			nsUnixMain.cpp

endif

CPPSRCS = \
	$(TOOLKIT_CPPSRCS) \
	nsBrowserWindow.cpp     \
	nsEditorMode.cpp        \
	nsEditorInterfaces.cpp  \
	nsSetupRegistry.cpp     \
	nsViewerApp.cpp         \
	nsWebCrawler.cpp        \
	nsStubs.cpp             \
	$(NULL)

INCLUDES += \
			-I$(PUBLIC)/raptor \
			-I$(PUBLIC)/xpcom \
			-I$(PUBLIC)/dom \
			-I$(PUBLIC)/netlib \
			-I$(PUBLIC)/js \
			-I$(PUBLIC)/pref \
			-I$(PUBLIC)/network \
			-I$(PUBLIC)/xp

ifdef MOZ_OJI
	JSJ_LIB = -ljsj
endif

EX_LIBS = \
	-L$(DIST)/bin \
	-lraptorbase    \
	-lpng \
	-lpref \
	-lraptorbase \
	$(TOOLKIT_WIDGET_LIB) \
	-lraptorgfx \
	$(TOOLKIT_GFX_LIB) \
	-lraptorhtml \
	$(DIST)/lib/libraptorhtmlforms_s.a \
	$(TOOLKIT_BASE_LIB) \
	-lraptorhtmlpars \
	-lraptorview \
	-lreg \
	-labouturl \
	-lfileurl \
	-lftpurl \
	-lgophurl \
	-lhttpurl \
	-limg \
	-ljpeg \
	-ljs \
	-ljsdom \
	$(DIST)/lib/libjsdomcore_s.a \
	$(JSJ_LIB) \
	-lmimetype \
	-lnetcache \
	-lnetcnvts \
	-lnetlib \
	-lnetutil \
	-lnetwork \
	-lnspr21 \
	-lplc21 \
	-lplds21 \
	-lraptorwebwidget \
	-lreg \
	-lremoturl \
	-lsecfree \
	-lstubnj \
	-lstubsj \
	-ltestdynamic \
	-lutil \
	-lxp \
	-lxpcom \
	-lzlib \
	-lutil \
	-lxp \
	-lxpcom \
	-lzlib \
	-lraptorplugin \
	$(NULL)

ifeq ($(OS_ARCH),Linux)
EX_LIBS += \
	-lpwcac \
	-ldbm \
	$(NULL)
endif

ifeq ($(OS_ARCH),SunOS)
EX_LIBS += \
	-lpwcac \
	-ldbm \
	$(NULL)
endif

PROGS = $(OBJDIR)/viewer

TARGETS = $(PROGS)

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
	TOOLKIT_LIB_PATH    = -L/usr/openwin/lib -L/usr/dt/lib
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

$(PROGS): $(OBJS) #$(EX_LIBS)
	@$(MAKE_OBJDIR)
	$(CCC) $(EXTRA_PRE_LDFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) $(TOOLKIT_LIB_PATH) $(TOOLKIT_LDFLAGS) $(EXTRA_POST_LDFLAGS)
	$(POST_LINK_COMMAND) $@

export::

install:: $(TARGETS)
	$(INSTALL) $(PROGS) $(DIST)/bin
	$(INSTALL) samples/test0.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test1.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test2.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test3.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test4.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test5.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test6.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test7.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test8.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test8siz.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test8sca.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test8tab.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test9.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test9a.html $(DIST)/bin/res/samples
	$(INSTALL) samples/test9b.html $(DIST)/bin/res/samples
	$(INSTALL) samples/raptor.jpg $(DIST)/bin/res/samples
	$(INSTALL) samples/Anieyes.gif $(DIST)/bin/res/samples
	$(INSTALL) samples/gear1.gif $(DIST)/bin/res/samples
	$(INSTALL) samples/rock_gra.gif $(DIST)/bin/res/samples
	$(INSTALL) samples/bg.jpg $(DIST)/bin/res/samples
	$(INSTALL) samples/test10.html $(DIST)/bin/res/samples
	$(INSTALL) throbber/anims00.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims01.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims02.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims03.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims04.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims05.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims06.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims07.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims08.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims09.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims10.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims11.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims12.gif $(DIST)/bin/res/throbber
	$(INSTALL) throbber/anims13.gif $(DIST)/bin/res/throbber

clobber::
	rm -f $(DIST)/bin/viewer.exe
	rm -f $(DIST)/bin/res/samples/test0.html
	rm -f $(DIST)/bin/res/samples/test1.html
	rm -f $(DIST)/bin/res/samples/test2.html
	rm -f $(DIST)/bin/res/samples/test3.html
	rm -f $(DIST)/bin/res/samples/test4.html
	rm -f $(DIST)/bin/res/samples/test5.html
	rm -f $(DIST)/bin/res/samples/test6.html
	rm -f $(DIST)/bin/res/samples/test7.html
	rm -f $(DIST)/bin/res/samples/test8.html
	rm -f $(DIST)/bin/res/samples/test8siz.html
	rm -f $(DIST)/bin/res/samples/test8sca.html
	rm -f $(DIST)/bin/res/samples/test8tab.html
	rm -f $(DIST)/bin/res/samples/test9.html
	rm -f $(DIST)/bin/res/samples/test9a.html
	rm -f $(DIST)/bin/res/samples/test9b.html
	rm -f $(DIST)/bin/res/samples/raptor.jpg
	rm -f $(DIST)/bin/res/samples/Anieyes.gif
	rm -f $(DIST)/bin/res/samples/gear1.gif
	rm -f $(DIST)/bin/res/samples/rock_gra.gif
	rm -f $(DIST)/bin/res/samples/bg.jpg
	rm -f $(DIST)/bin/res/samples/test10.html
	rm -f $(DIST)/bin/res/throbber/anims00.gif
	rm -f $(DIST)/bin/res/throbber/anims01.gif
	rm -f $(DIST)/bin/res/throbber/anims02.gif
	rm -f $(DIST)/bin/res/throbber/anims03.gif
	rm -f $(DIST)/bin/res/throbber/anims04.gif
	rm -f $(DIST)/bin/res/throbber/anims05.gif
	rm -f $(DIST)/bin/res/throbber/anims06.gif
	rm -f $(DIST)/bin/res/throbber/anims07.gif
	rm -f $(DIST)/bin/res/throbber/anims08.gif
	rm -f $(DIST)/bin/res/throbber/anims09.gif
	rm -f $(DIST)/bin/res/throbber/anims10.gif
	rm -f $(DIST)/bin/res/throbber/anims11.gif
	rm -f $(DIST)/bin/res/throbber/anims12.gif
	rm -f $(DIST)/bin/res/throbber/anims13.gif


