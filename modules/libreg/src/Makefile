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

DEPTH		= ../../..

MODULE		= libreg
LIBRARY_NAME	= reg

REQUIRES	= libreg nspr pref js

CSRCS		= reg.c VerReg.c vr_stubs.c

BIN_SRCS	= VerReg.c reg.c vr_stubs.c
BIN_OBJS	= $(addprefix $(OBJDIR)/R_,$(BIN_SRCS:.c=.o))

include $(DEPTH)/config/rules.mk

# Override gtscc when building vreg on IRIX 5.3.
ifeq ($(OS_ARCH)$(OS_RELEASE),IRIX5)
CC		= cc
endif

INCLUDES	+= -I$(DEPTH)/include
GARBAGE		+= $(PROGRAM)

PROGRAM		= $(OBJDIR)/vreg$(BIN_SUFFIX)

$(PROGRAM): $(BIN_OBJS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), OS2)
	$(LINK_EXE) -OUT:$@ $(BIN_OBJS) $(LDFLAGS) $(OS_LIBS)
else
	$(CCF) -o $@ $(BIN_OBJS) $(LDFLAGS)
endif	

$(OBJDIR)/R_VerReg.o:	VerReg.c
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), OS2)
	$(CC) -Fo$@ -c $(CFLAGS) -DSTANDALONE_REGISTRY $<
else
	$(CC) -o $@ -c $(CFLAGS) -DSTANDALONE_REGISTRY $<
endif

$(OBJDIR)/R_reg.o:	reg.c
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), OS2)
	$(CC) -Fo$@ -c $(CFLAGS) -DSTANDALONE_REGISTRY $<
else
	$(CC) -o $@ -c $(CFLAGS) -DSTANDALONE_REGISTRY $<
endif

$(OBJDIR)/R_vr_stubs.o:	vr_stubs.c
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), OS2)
	$(CC) -Fo$@ -c $(CFLAGS) -DSTANDALONE_REGISTRY $<
else
	$(CC) -o $@ -c $(CFLAGS) -DSTANDALONE_REGISTRY $<
endif

$(BIN_OBJS): $(BIN_SRCS)

install:: $(PROGRAM)
	$(INSTALL) -m 444 $(PROGRAM) $(DIST)/bin
