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

DEPTH = ../..

include $(DEPTH)/config/config.mk

DIRS = unix 

MODULE=shell

LIBRARY_NAME = shell

DEFINES = -D_IMPL_NS_SHELL

EXTRA_DSO_LDOPTS+= -L$(DIST)/lib \
		$(DIST)/bin/libxpcom.so \
		$(DIST)/bin/libnspr21.so \
		$(DIST)/bin/libplc21.so \
		$(DIST)/bin/libplds21.so \
		$(DIST)/bin/libnspr21.so \
		$(DIST)/bin/libreg.so \
		$(NULL) \

ifeq ($(OS_ARCH),linux)
EXTRA_DSO_LDOPTS+= /usr/lib/libc.a /usr/lib/libstdc++.a 
endif

# This should really be done properly in 'config'
ifeq ($(OS_ARCH),Linux)
EXTRA_DSO_LDOPTS+=  -L/usr/X11R6/lib -lXm -lXt -lX11
else
EXTRA_DSO_LDOPTS+= -lXm -lXt -lX11
endif

LCFLAGS=-I$(PUBLIC)/xpcom -I$(PUBLIC)/raptor \
      -I$(PUBLIC)/dom -I$(PUBLIC)/js -I$(PUBLIC)/netlib -I$(PUBLIC)/shell

EXPORTS =				\
 nsApplicationManager.h			\
 nsShellInstance.h			\
 $(NULL)

CPPSRCS=				\
 nsApplicationManager.cpp			\
 nsShellInstance.cpp			\
 $(NULL)

CPP_OBJS=				\
 ./$(OBJDIR)/nsApplicationManager.o		\
 ./$(OBJDIR)/nsShellInstance.o		\
 $(NULL)


TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk
