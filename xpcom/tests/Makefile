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

DEPTH		= ../..

DIRS		= dynamic

CPPSRCS		= RegFactory.cpp TestFactory.cpp

include $(DEPTH)/config/rules.mk

CFLAGS		+= -DUSE_NSREG
INCLUDES	= -I../public -I$(DIST)/include

EX_LIBS		= $(DIST)/lib/libxpcom.$(LIB_SUFFIX) \
		  $(DIST)/lib/libreg.$(LIB_SUFFIX) \
		  $(DIST)/lib/libplc21.$(LIB_SUFFIX) \
		  $(DIST)/lib/libplds21.$(LIB_SUFFIX) \
		  $(DIST)/lib/libnspr21.$(LIB_SUFFIX) \
		  $(NULL)

PROGS		= $(addprefix $(OBJDIR)/, $(CPPSRCS:.cpp=))

TARGETS		= $(PROGS)

ifeq ($(OS_ARCH),Rhapsody)
$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o $(EX_LIBS)
	@$(MAKE_OBJDIR)
	$(CC) -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS)
else
$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o $(EX_LIBS)
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS)
endif

install:: $(PROGS)
	$(INSTALL) $(PROGS) $(DIST)/bin
