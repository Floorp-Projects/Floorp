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

DEPTH=../..

include $(DEPTH)/config/config.mk

CPPSRCS = \
	RegFactory.cpp \
	TestFactory.cpp	\
	$(NULL)

INCLUDES=-I../src -I$(DIST)/include

DIRS = dynamic 

OBJS	= $(CPPSRCS:.cpp=.o)

EX_LIBS = \
        $(DIST)/bin/libreg.so	\
	$(DIST)/bin/libxpcom.so	\
	$(DIST)/lib/libplc21.a	\
	$(DIST)/lib/libplds21.a	\
	$(DIST)/lib/libnspr21.a	\
	$(NULL)

PROGS	= $(addprefix $(OBJDIR)/, $(CPPSRCS:.cpp=))

NON_DIRS = $(PROGS)
TARGETS = $(NON_DIRS)

include $(DEPTH)/config/rules.mk

$(OBJDIR)/%.o: %.cpp
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $(CFLAGS) -DUSE_NSREG -c $*.cpp

$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o $(EX_LIBS)
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS)

export::

install:: $(PROGS)
	$(INSTALL) $(PROGS) $(DIST)/bin

