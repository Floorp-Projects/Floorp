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
IGNORE_MANIFEST=1

MAKE_OBJ_TYPE=EXE
PROGRAM=./$(OBJDIR)/viewer


include $(DEPTH)/config/config.mk

CPPSRCS = \
	main.cpp \
        nsViewer.cpp \
	nsDocLoader.cpp \
	$(NULL)

INCLUDES+= -I$(PUBLIC)/raptor -I$(PUBLIC)/xpcom  -I$(PUBLIC)/dom -I$(PUBLIC)/netlib -I$(PUBLIC)/js

DIRS	=

OBJS	= $(CPPSRCS:.cpp=.o)

EX_LIBS = \
	$(DIST)/bin/libwidgetunix.so	\
	$(DIST)/bin/libgfxunix.so	\
	$(DIST)/bin/libraptorbase.so	\
	$(DIST)/bin/libxpcom.so	\
	$(DIST)/bin/libreg.so	\
	$(DIST)/bin/libplc21.so	\
	$(DIST)/bin/libplds21.so	\
	$(DIST)/bin/libnspr21.so	\
	$(NULL)

#PROGS	= $(addprefix $(OBJDIR)/, $(CPPSRCS:.cpp=))

#TARGETS = $(PROGS)

include $(DEPTH)/config/rules.mk

#$(OBJDIR)/%.o: %.cpp
#	@$(MAKE_OBJDIR)
#	$(CCC) -o $@ $(CFLAGS) -c $*.cpp

#$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o $(EX_LIBS)
#	@$(MAKE_OBJDIR)
#	$(CCC) -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -lXm -lXt -lX11


install:: $(PROGRAM)
	$(MAKE_INSTALL) $(PROGRAM) $(DIST)/bin
	$(MAKE_INSTALL) samples/test0.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test1.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test2.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test3.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test4.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test5.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test6.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test7.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test8.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test8siz.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test8sca.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/test8tab.html $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/raptor.jpg $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/Anieyes.gif $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/gear1.gif $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/rock_gra.gif $(DIST)/bin/res/samples
	$(MAKE_INSTALL) samples/bg.jpg $(DIST)/bin/res/samples

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
	rm -f $(DIST)/bin/res/samples/raptor.jpg
	rm -f $(DIST)/bin/res/samples/Anieyes.gif
	rm -f $(DIST)/bin/res/samples/gear1.gif
	rm -f $(DIST)/bin/res/samples/rock_gra.gif
	rm -f $(DIST)/bin/res/samples/bg.jpg

