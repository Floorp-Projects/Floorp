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
#

#! gmake

MOD_DEPTH = ../../..

include $(MOD_DEPTH)/config/config.mk

# Disable optimization of the nspr on SunOS4.1.3
ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
OPTIMIZER =
endif
endif

CSRCS =            \
	pralarm.c  \
	pratom.c   \
	prdtoa.c   \
	prenv.c    \
	prerror.c  \
	prinit.c   \
	prinrval.c \
	prlog2.c   \
	prlong.c   \
	prnetdb.c  \
	prsystem.c \
	prtime.c   \
	prthinfo.c \
	$(NULL)

TARGETS	= $(OBJS)

INCLUDES = -I$(DIST)/include/private -I$(DIST)/include

include $(MOD_DEPTH)/config/rules.mk

# An AIX Optimization bug causes PR_dtoa() to produce wrong result.
# This suppresses optimization for this single compilation unit.
ifeq ($(OS_ARCH), AIX)
$(OBJDIR)/prdtoa.o:
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(filter-out -O, $(CFLAGS)) prdtoa.c
endif

export:: $(TARGETS)

install:: export

