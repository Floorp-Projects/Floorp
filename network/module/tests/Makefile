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
	nettest.cpp \
	$(NULL)

INCLUDES+=-I../src -I$(DIST)/include

REQUIRES = raptor netlib xpcom

OBJS	= $(CPPSRCS:.cpp=.o)

EX_LIBS = \
	$(DIST)/bin/libabouturl.so \
	$(DIST)/bin/libfileurl.so \
	$(DIST)/bin/libftpurl.so \
	$(DIST)/bin/libgophurl.so \
	$(DIST)/bin/libremoturl.so \
	$(DIST)/bin/libhttpurl.so \
	$(DIST)/bin/libnetcache.so \
	$(DIST)/bin/libmimetype.so \
	$(DIST)/bin/libnetwork.so \
	$(DIST)/bin/libnetlib.so \
	$(DIST)/bin/libnetutil.so \
        $(DIST)/bin/libreg.so	\
	$(DIST)/bin/libxpcom.so	\
	$(DIST)/bin/libpwcac.so \
	$(DIST)/bin/libdbm.so	\
	$(DIST)/bin/libxp.so	\
	$(DIST)/bin/libpref.so	\
	$(DIST)/bin/libjs.so	\
	$(DIST)/bin/libraptorbase.so \
	$(DIST)/bin/libsecfree.so \
	$(DIST)/lib/libplc21.so \
	$(DIST)/lib/libplds21.so	\
	$(DIST)/lib/libnspr21.so \
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

