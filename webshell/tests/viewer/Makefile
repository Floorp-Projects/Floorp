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

CPPSRCS = \
	nsBrowserWindow.cpp     \
	nsEditorMode.cpp        \
	nsEditorInterfaces.cpp  \
	nsMotifMenu.cpp         \
	nsSetupRegistry.cpp     \
	nsUnixMain.cpp          \
	nsViewerApp.cpp         \
	nsWebCrawler.cpp        \
	nsStubs.cpp             \
	$(NULL)

INCLUDES+= -I$(PUBLIC)/raptor -I$(PUBLIC)/xpcom  -I$(PUBLIC)/dom \
	-I$(PUBLIC)/netlib -I$(PUBLIC)/js -I$(PUBLIC)/pref -I$(PUBLIC)/network -I$(PUBLIC)/xp

OBJS    = $(CPPSRCS:.cpp=.o)

ifeq ($(USE_NGLAYOUT_GTK),1)
	GFX_LIB    = -lgfxgtk
	WIDGET_LIB = -lwidgetgtk
	BASE_DSO_FLAGS = -lgmbasegtk
else
	GFX_LIB    = -lgfxmotif
	WIDGET_LIB = -lwidgetmotif
	BASE_DSO_FLAGS = -lgmbasemotif
endif

ifdef MOZ_OJI
	JSJ_LIB = -ljsj
endif

EX_LIBS = \
	-L$(DIST)/bin \
	-lraptorbase    \
	-lpng \
	-lpref \
	-lraptorbase \
	$(WIDGET_LIB) \
	-lraptorgfx \
	$(GFX_LIB) \
	-lraptorhtml \
	$(DIST)/lib/libraptorhtmlforms_s.a \
	$(BASE_DSO_FLAGS) \
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

$(PROGS): $(OBJS) #$(EX_LIBS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),Linux)
	$(CCC) -rdynamic -o $@ $(OBJS) $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -L/usr/X11R6/lib -lXm -lXt -lX11 -lXp -lXext
else
ifeq ($(OS_ARCH),IRIX)
	$(CCC) -o $@ -woff 84,85 $(LDFLAGS) $(OBJS) $(EX_LIBS) $(OS_LIBS) -lXm -lXt -lX11 
else
	$(CCC) -o $@ $(LDFLAGS) $(OBJS) $(EX_LIBS) $(OS_LIBS) -lXm -lXt -lX11 
endif
endif

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


