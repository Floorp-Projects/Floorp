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
	nsFindDialog.cpp \
	nsSetupRegistry.cpp	\
	nsUnixStubs.cpp  		\
	nsBrowserWindow.cpp	\
	nsBrowserMain.cpp	\
	nsViewerApp.cpp		\
	nsXPBaseWindow.cpp \
	$(NULL)

CFLAGS += $(TOOLKIT_CFLAGS)

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

OBJS	= $(CPPSRCS:.cpp=.o)

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

ifneq (,$(filter Linux SunOS,$(OS_ARCH)))
EX_LIBS += \
	-lpwcac \
	-ldbm \
	$(NULL)
endif

ifneq (,$(filter Linux SunOS,$(OS_ARCH)))
EX_LIBS += \
	-lpwcac \
	$(NULL)
endif

PROGS = $(OBJDIR)/xpviewer

TARGETS = $(PROGS)

include $(DEPTH)/config/rules.mk

$(PROGS): $(OBJS) # $(EX_LIBS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),Linux)
	$(CCC) -rdynamic -o $@ $(OBJS) $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -L/usr/X11R6/lib -lXm -lXt -lX11 -lXp -lXext
else
	$(CCC) -o $@ -woff 84,85 $(LDFLAGS) $(OBJS) $(EX_LIBS) $(OS_LIBS) -lXm -lXt -lX11 
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
	$(INSTALL) resources/throbber/LargeAnimation00.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation01.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation02.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation03.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation04.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation05.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation06.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation07.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation08.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation09.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation10.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation11.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation12.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation13.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation14.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation15.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation16.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation17.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation18.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation19.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation20.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation21.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation22.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation23.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation24.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation25.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation26.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation27.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation28.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation29.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation30.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation31.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation32.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation33.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation34.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation35.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation36.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation37.gif $(DIST)/bin/res/throbber
	$(INSTALL) resources/throbber/LargeAnimation38.gif $(DIST)/bin/res/throbber

	$(INSTALL) resources/toolbar/DialogAddrIcon.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/DialogAddrIcon_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/DialogCompIcon.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/DialogCompIcon_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/DialogMailIcon.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/DialogMailIcon_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/DialogNavIcon.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/DialogNavIcon_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Back.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Back_dis.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Back_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Bookmarks.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Bookmarks_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Edit.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Forward.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Forward_dis.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Forward_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Home.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Home_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_HTab.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_HTab_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_LoadImages.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_LoadImages.mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Location.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Location_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_MiniAddr.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_MiniComp.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_MiniMail.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_MiniNav.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_MiniTab.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_MixSecurity.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_MixSecurity.mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Netscape.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Netscape_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_PersonalIcon.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Places.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Places_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Print.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Print_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Reload.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Reload_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Search.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Search_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Secure.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Secure_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Stop.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Stop.mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Stop_dis.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Stop_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Tab.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_TabSmall.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_TabSmall_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Tab_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Unsecure.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_Unsecure.mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_WhatsRelated.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/TB_WhatsRelated_mo.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/StatusBar-insecure.gif $(DIST)/bin/res/toolbar
	$(INSTALL) resources/toolbar/StatusBar-secure.gif $(DIST)/bin/res/toolbar

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


