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

MOD_DEPTH = ../..

include $(MOD_DEPTH)/config/config.mk

# Disable optimization of the nspr on SunOS4.1.3
ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
OPTIMIZER =
endif
endif

INCLUDES = -I$(DIST)/include -I$(MOD_DEPTH)/pr/include

CSRCS = \
	plarena.c \
	plhash.c \
	plvrsion.c \
	$(NULL)

HEADERS = \
	plarenas.h \
	plarena.h \
	plhash.h \
	$(NULL)

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
# OS_CFLAGS = $(OS_EXE_CFLAGS)
EXTRA_LIBS = $(DIST)/lib/nspr$(MOD_VERSION).$(LIB_SUFFIX)
else
DLLBASE=/BASE:0x30000000
RES=$(OBJDIR)/plds.res
RESNAME=plds.rc
OS_LIBS = user32.lib
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
ifeq ($(MOZ_OS2_TOOLS),VACPP)
EXTRA_LIBS = $(DIST)/lib/nspr$(MOD_VERSION).$(LIB_SUFFIX)

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

LIBRARY_NAME	= plds
LIBRARY_VERSION	= $(MOD_VERSION)

RELEASE_HEADERS = $(HEADERS)
RELEASE_HEADERS_DEST = $(RELEASE_INCLUDE_DIR)
RELEASE_LIBS	= $(TARGETS)

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
	SUF = i64
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
	$(CC) -Fo$@ -c $(CFLAGS) -I$(OBJDIR) plvrsion.c
else
ifeq ($(MOZ_OS2_TOOLS), VACPP)
	$(CC) -Fo$@ -c $(CFLAGS) -I$(OBJDIR) plvrsion.c
else
	$(CC) -o $@ -c $(CFLAGS) -I$(OBJDIR) plvrsion.c
endif
endif
#
# Version information generation (end)
#

#
# The Client build wants the shared libraries in $(DIST)/bin,
# so we also install them there.
#

export:: $(TARGETS)
	$(INSTALL) -m 444 $(HEADERS) $(DIST)/include
	$(INSTALL) -m 444 $(TARGETS) $(DIST)/lib
ifdef SHARED_LIBRARY
ifeq ($(OS_ARCH),HP-UX)
	$(INSTALL) -m 755 $(SHARED_LIBRARY) $(DIST)/lib
	$(INSTALL) -m 755 $(SHARED_LIBRARY) $(DIST)/bin
else
	$(INSTALL) -m 444 $(SHARED_LIBRARY) $(DIST)/bin
endif
ifeq ($(OS_ARCH), OpenVMS)
	$(INSTALL) -m 444 $(SHARED_LIBRARY:.$(DLL_SUFFIX)=.vms) $(DIST)/lib
endif
endif
ifeq ($(MOZ_BITS),16)
	$(INSTALL) -m 444 $(HEADERS) $(MOZ_INCL)
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/lib
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/bin
endif

install:: export

