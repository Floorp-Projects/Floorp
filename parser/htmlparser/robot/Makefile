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

DEPTH = ../../..

include $(DEPTH)/config/config.mk

CPPSRCS =			\
	RobotMain.cpp		\
	nsRobotSink.cpp		\
	$(NULL)

REQUIRES=xpcom raptor

OBJS	= $(CPPSRCS:.cpp=.o)

EX_LIBS = \
	$(DIST)/lib/libraptorbase.a \
	$(DIST)/lib/libraptorhtmlpars.a \
	$(DIST)/lib/libnetlib.a \
	$(DIST)/lib/libpref.a	\
	$(DIST)/lib/libxp.a	\
	$(DIST)/lib/libjs.a	\
	$(DIST)/lib/libxpcom.a  \
	$(DIST)/lib/libplc21.a	\
	$(DIST)/lib/libplds21.a	\
	$(DIST)/lib/libnspr21.a	\
	-lXm			\
	-lm			\
	$(NULL)

PROG = htmlrobot

$(PROG):$(OBJS)
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $(OBJS) $(LDFLAGS) $(EX_LIBS) $(OS_LIBS)

TARGETS = $(PROG)

include $(DEPTH)/config/rules.mk
