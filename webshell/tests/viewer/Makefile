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
	main.cpp \
	$(NULL)

INCLUDES+= -I$(PUBLIC)/raptor -I$(PUBLIC)/xpcom  -I$(PUBLIC)/dom -I$(PUBLIC)/netlib -I$(PUBLIC)/js

DIRS	=

OBJS	= $(CPPSRCS:.cpp=.o)



EX_LIBS = \
	$(DIST)/bin/libraptorbase.so	\
	$(DIST)/bin/libxpcom.so	\
	$(DIST)/bin/libreg.so	\
	$(DIST)/bin/libplc21.so	\
	$(DIST)/bin/libplds21.so	\
	$(DIST)/bin/libnspr21.so	\
	$(DIST)/bin/libnetlib.so \
	$(NULL)


#EX_LIBS = \
#	$(DIST)/bin/libraptorhtml.so \
#	$(DIST)/bin/libnetlib.so \
#	$(DIST)/bin/libraptorbase.so	\
#	$(DIST)/bin/libxpcom.so	\
#	$(DIST)/bin/libreg.so	\
#	$(DIST)/bin/libplc21.so	\
#	$(DIST)/bin/libplds21.so	\
#	$(DIST)/bin/libnspr21.so	\
#	$(NULL)

PROGS	= $(addprefix $(OBJDIR)/, $(CPPSRCS:.cpp=))

TARGETS = $(PROGS)


include $(DEPTH)/config/rules.mk

nsDocLoader.o::	nsDocLoader.cpp
	$(CCC) -c $(CFLAGS) nsDocLoader.cpp $(LDFLAGS) 

nsViewer.o::	nsViewer.cpp
	$(CCC) -c $(CFLAGS) nsViewer.cpp $(LDFLAGS) 


$(OBJDIR)/%.o: %.cpp
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $(CFLAGS) -c $*.cpp

$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o nsViewer.o nsDocLoader.o 
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $@.o nsViewer.o nsDocLoader.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS) /usr/X11R6/lib/libXm.a  /usr/X11R6/lib/libXt.a /usr/X11R6/lib/libX11.a


export::

install:: $(TARGETS)
	$(INSTALL) $(PROGS) $(DIST)/bin

