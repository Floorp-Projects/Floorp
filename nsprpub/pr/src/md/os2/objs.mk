#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "NPL"); you may not use this file except in
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
#

# This makefile appends to the variable OBJS the platform-dependent
# object modules that will be part of the nspr20 library.

CSRCS = \
	os2io.c      \
	os2sock.c    \
	os2thred.c   \
	os2cv.c      \
	os2gc.c      \
	os2misc.c    \
	os2inrval.c  \
	os2sem.c     \
	os2_errors.c \
	os2poll.c    \
	os2rng.c     \
	$(NULL)

ifeq ($(MOZ_OS2_TOOLS),VACPP)
ASFILES = os2vacpp.asm
endif

OBJS += $(addprefix md/os2/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/os2/$(OBJDIR)/,$(ASFILES:.asm=.$(OBJ_SUFFIX)))

