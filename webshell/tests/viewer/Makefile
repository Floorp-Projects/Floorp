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
	nsGtkMain.cpp          \
	nsViewerApp.cpp         \
	nsWebCrawler.cpp        \
	nsStubs.cpp             \
	$(NULL)

INCLUDES+= -I$(PUBLIC)/raptor -I$(PUBLIC)/xpcom  -I$(PUBLIC)/dom \
	-I$(PUBLIC)/netlib -I$(PUBLIC)/js -I$(PUBLIC)/pref -I$(PUBLIC)/network -I$(PUBLIC)/xp

OBJS    = $(CPPSRCS:.cpp=.o)

EX_LIBS = \
	$(DIST)/bin/libraptorbase.so    \
	$(DIST)/bin/libpng.so \
	$(DIST)/bin/libpref.so \
	$(DIST)/bin/libraptorbase.so \
	$(DIST)/bin/libwidgetunix.so \
	$(DIST)/bin/libraptorgfx.so \
	$(DIST)/bin/libgfxmotif.so \
	$(DIST)/bin/libraptorhtml.so \
	$(DIST)/bin/libgmbaseunix.so \
	$(DIST)/bin/libraptorhtmlpars.so \
	$(DIST)/bin/libraptorview.so \
	$(DIST)/bin/libreg.so \
	$(DIST)/bin/libabouturl.so \
	$(DIST)/bin/libfileurl.so \
	$(DIST)/bin/libftpurl.so \
	$(DIST)/bin/libgophurl.so \
	$(DIST)/bin/libhttpurl.so \
	$(DIST)/bin/libimg.so \
	$(DIST)/bin/libjpeg.so \
	$(DIST)/bin/libjs.so \
	$(DIST)/bin/libjsdom.so \
	$(DIST)/bin/libjsj.so \
	$(DIST)/bin/libmimetype.so \
	$(DIST)/bin/libnetcache.so \
	$(DIST)/bin/libnetcnvts.so \
	$(DIST)/bin/libnetlib.so \
	$(DIST)/bin/libnetutil.so \
	$(DIST)/bin/libnetwork.so \
	$(DIST)/bin/libnspr21.so \
	$(DIST)/bin/libplc21.so \
	$(DIST)/bin/libplds21.so \
	$(DIST)/bin/libraptorwebwidget.so \
	$(DIST)/bin/libreg.so \
	$(DIST)/bin/libremoturl.so \
	$(DIST)/bin/libsecfree.so \
	$(DIST)/bin/libstubnj.so \
	$(DIST)/bin/libstubsj.so \
	$(DIST)/bin/libtestdynamic.so \
	$(DIST)/bin/libutil.so \
	$(DIST)/bin/libxp.so \
	$(DIST)/bin/libxpcom.so \
	$(DIST)/bin/libzlib.so \
	$(DIST)/bin/libutil.so \
	$(DIST)/bin/libxp.so \
	$(DIST)/bin/libxpcom.so \
	$(DIST)/bin/libzlib.so \
	$(DIST)/bin/libraptorplugin.so \
	$(NULL)

ifeq ($(OS_ARCH),Linux)
EX_LIBS += \
	$(DIST)/bin/libpwcac.so \
	$(DIST)/bin/libdbm.so \
	$(NULL)
endif

ifeq ($(OS_ARCH),SunOS)
EX_LIBS += \
	$(DIST)/bin/libpwcac.so \
	$(DIST)/bin/libdbm.so \
	$(NULL)
endif

PROGS = $(OBJDIR)/viewer

TARGETS = $(PROGS)

include $(DEPTH)/config/rules.mk

$(PROGS): $(OBJS) $(EX_LIBS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),Linux)
	$(CCC) -rdynamic -o $@ $(OBJS) $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -L/usr/X11R6/lib -lXm -lXpm -lXt -lX11 -lXp -lXext
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


