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
	main.cpp		\
	$(NULL)


INCLUDES+= -I$(PUBLIC)/raptor -I$(PUBLIC)/xpcom  -I$(PUBLIC)/dom -I$(PUBLIC)/netlib -I$(PUBLIC)/js

DIRS	=

OBJS	= $(CPPSRCS:.cpp=.o)

EX_LIBS = \
	$(DIST)/bin/libraptorbase.so	\
	$(DIST)/bin/libpng.so \
	$(DIST)/bin/libpref.so \
	$(DIST)/bin/libraptorbase.so \
	$(DIST)/bin/libraptorgfx.so \
	$(DIST)/bin/libraptorhtml.so \
	$(DIST)/bin/libgmbaseunix.so \
	$(DIST)/bin/libraptorhtmlbase_s.so \
	$(DIST)/bin/libraptorhtmldoc_s.so \
	$(DIST)/bin/libraptorhtmlforms_s.so \
	$(DIST)/bin/libraptorhtmlpars.so \
	$(DIST)/bin/libraptorhtmlstyle_s.so \
	$(DIST)/bin/libraptorhtmltable_s.so \
	$(DIST)/bin/libraptorlayout_s.so \
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
	$(DIST)/bin/libmsgc21.so \
	$(DIST)/bin/libnetcache.so \
	$(DIST)/bin/libnetcnvts.so \
	$(DIST)/bin/libnetlib.so \
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
	$(DIST)/bin/libpwcac.so \
	$(DIST)/bin/libdbm.so \
	$(NULL)

PROGS   = $(addprefix $(OBJDIR)/, $(CPPSRCS:.cpp=))

TARGETS = $(PROGS)

include $(DEPTH)/config/rules.mk

$(OBJDIR)/nsDocLoader.o::	nsDocLoader.cpp
	$(CCC) -o $@ -c $(CFLAGS) nsDocLoader.cpp $(LDFLAGS) 

$(OBJDIR)/nsViewer.o::	nsViewer.cpp
	$(CCC) -o $@ -c $(CFLAGS) nsViewer.cpp $(LDFLAGS) 


$(OBJDIR)/%.o: %.cpp
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $(CFLAGS) -c $*.cpp

$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o $(OBJDIR)/nsDocLoader.o $(OBJDIR)/nsViewer.o $(EX_LIBS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),Linux)
	$(CCC) -rdynamic -o $@ $@.o $(OBJDIR)/nsDocLoader.o $(OBJDIR)/nsViewer.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -L/usr/X11R6/lib -lXm -lXt -lX11 -lXp -lXext
else
	$(CCC) -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -lXm -lXt -lX11 
endif

export::

install:: $(TARGETS)
	$(INSTALL) $(PROGS) $(DIST)/bin



