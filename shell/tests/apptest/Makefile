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
	nsAppTest.cpp		\
	$(NULL)

INCLUDES+= -I$(PUBLIC)/raptor -I$(PUBLIC)/xpcom -I$(PUBLIC)/shell

DIRS	=

OBJS	= $(CPPSRCS:.cpp=.o)

EX_LIBS = $(DIST)/lib/libshell_s.a \
	$(DIST)/bin/libxpcom.so	\
	$(DIST)/bin/libreg.so	\
	$(DIST)/bin/libgfxunix.so	\
	$(DIST)/bin/libraptorbase.so	\
	$(DIST)/bin/libraptorgfx.so	\
	$(DIST)/bin/libimg.so	\
	$(DIST)/lib/libplc21.a	\
	$(DIST)/lib/libplds21.a	\
	$(DIST)/lib/libnspr21.a	\
	$(DIST)/bin/libshell.so \
	$(NULL)


PROGS	= $(addprefix $(OBJDIR)/, $(CPPSRCS:.cpp=))

TARGETS = $(PROGS)

include $(DEPTH)/config/rules.mk

$(OBJDIR)/%.o: %.cpp
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $(CFLAGS) -c $*.cpp

$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o $(EX_LIBS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),Linux)
	$(CCC) -rdynamic -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -L/usr/X11R6/lib -lXm -lXt -lX11 -lXp -lXext
else
	$(CCC) -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) -lXm -lXt -lX11 
endif

export::

install:: $(TARGETS)
	$(INSTALL) $(PROGS) $(DIST)/bin



