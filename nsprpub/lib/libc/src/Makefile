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

#! gmake

MOD_DEPTH = ../../..

include $(MOD_DEPTH)/config/config.mk

# Disable optimization of the nspr on SunOS4.1.3
ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
OPTIMIZER =
endif
endif

INCLUDES = -I$(DIST)/include

CSRCS =\
	plvrsion.c  \
	strlen.c  \
	strcpy.c  \
	strdup.c  \
	strcat.c  \
	strcmp.c  \
	strccmp.c \
	strchr.c  \
	strpbrk.c \
	strstr.c  \
	strcstr.c \
	base64.c \
	plerror.c \
	plgetopt.c \
	$(NULL)

LIBRARY_NAME	= plc
LIBRARY_VERSION	= $(MOD_VERSION)

RELEASE_LIBS = $(TARGETS)

ifeq ($(OS_ARCH),WINNT)
ifeq (,$(filter-out WIN16 OS2,$(OS_TARGET)))
ifdef XP_OS2_EMX
EXTRA_LIBS = -L$(DIST)/lib -lnspr$(MOD_VERSION)
else
EXTRA_LIBS = $(DIST)/lib/nspr$(MOD_VERSION).lib
endif
else
DLLBASE=/BASE:0x30000000
RES=$(OBJDIR)/plc.res
RESNAME=$(MOD_DEPTH)/pr/src/nspr.rc
ifeq ($(OS_TARGET),WIN95)
EXTRA_LIBS = $(DIST)/lib/nspr$(MOD_VERSION).lib
else
EXTRA_LIBS = $(DIST)/lib/libnspr$(MOD_VERSION).lib
endif

ifdef MOZ_DEBUG
ifdef GLOWCODE
EXTRA_LIBS += $(GLOWDIR)/glowcode.lib
endif
endif

endif
else
ifeq ($(OS_ARCH), AIX)
ifeq ($(CLASSIC_NSPR),1)
OS_LIBS += -lc
else
OS_LIBS += -lc_r
endif
endif
ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
EXTRA_LIBS = -L$(DIST)/lib -lnspr$(MOD_VERSION)_shr
else
EXTRA_LIBS = -L$(DIST)/lib -lnspr$(MOD_VERSION)
endif
endif

# On NCR and SCOOS, we can't link with extra libraries when
# we build a shared library.  If we do so, the linker doesn't
# complain, but we would run into weird problems at run-time.
# Therefore on these platforms, we link just the .o files.
ifeq ($(OS_ARCH),NCR)
EXTRA_LIBS =
endif
ifeq ($(OS_ARCH),SCOOS)
EXTRA_LIBS =
endif

include $(MOD_DEPTH)/config/rules.mk

#
# Version information generation (begin)
#
ECHO = echo
TINC = $(OBJDIR)/_pl_bld.h
PROD = $(notdir $(SHARED_LIBRARY))
NOW = $(MOD_DEPTH)/config/$(OBJDIR)/now
SH_DATE = $(shell date)
SH_NOW = $(shell $(NOW))

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET),OS2)
	SUF =
else
	SUF = i64
endif
else
	SUF = LL
endif

$(TINC):
	@$(MAKE_OBJDIR)
	@$(ECHO) '#define _BUILD_STRING "$(SH_DATE)"' > $(TINC)
	@if test ! -z "$(SH_NOW)"; then \
	    $(ECHO) '#define _BUILD_TIME $(SH_NOW)$(SUF)' >> $(TINC); \
	else \
	    true; \
	fi
	@$(ECHO) '#define _PRODUCTION "$(PROD)"' >> $(TINC)


$(OBJDIR)/plvrsion.$(OBJ_SUFFIX): plvrsion.c $(TINC)
ifeq ($(OS_ARCH), WINNT)
ifdef XP_OS2_EMX
	$(CC) -o $@ -c $(CFLAGS) -I$(OBJDIR) plvrsion.c
else
	$(CC) -Fo$@ -c $(CFLAGS) -I$(OBJDIR) plvrsion.c
endif
else
	$(CC) -o $@ -c $(CFLAGS) -I$(OBJDIR) plvrsion.c
endif
#
# Version information generation (end)
#

#
# The Client build wants the shared libraries in $(DIST)/bin,
# so we also install them there.
#

export:: $(TARGETS)
	$(INSTALL) -m 444 $(TARGETS) $(DIST)/lib
ifdef SHARED_LIBRARY
	$(INSTALL) -m 444 $(SHARED_LIBRARY) $(DIST)/bin
endif
ifeq ($(MOZ_BITS),16)
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/lib
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/bin
endif

install:: export
