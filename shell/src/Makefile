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

DIRS = unix 

MODULE=shell

#LIBRARY_NAME = shell_s
LIBRARY_NAME = shell

DEFINES = -D_IMPL_NS_SHELL

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

include $(DEPTH)/config/config.mk

TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk
