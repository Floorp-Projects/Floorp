# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
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

INCLUDES = -I$(includedir)

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
ifeq ($(OS_TARGET), WIN16)
EXTRA_LIBS = $(libdir)/nspr$(MOD_VERSION).lib
else
DLLBASE=/BASE:0x30000000
RES=$(OBJDIR)/plc.res
RESNAME=plc.rc
ifeq ($(OS_TARGET),WIN95)
EXTRA_LIBS = $(libdir)/nspr$(MOD_VERSION).lib
else
EXTRA_LIBS = $(libdir)/libnspr$(MOD_VERSION).lib
endif

ifdef MOZ_DEBUG
ifdef GLOWCODE
EXTRA_LIBS += $(GLOWDIR)/glowcode.lib
endif
endif
endif

else
ifeq ($(MOZ_OS2_TOOLS),VACPP)
EXTRA_LIBS = $(libdir)/nspr$(MOD_VERSION).$(LIB_SUFFIX)

else
ifeq ($(OS_ARCH), AIX)
ifeq ($(CLASSIC_NSPR),1)
OS_LIBS += -lc
else
OS_LIBS += -lc_r
endif
endif
ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
EXTRA_LIBS = -L$(libdir) -lnspr$(MOD_VERSION)_shr
else
EXTRA_LIBS = -L$(libdir) -lnspr$(MOD_VERSION)
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

GARBAGE += $(TINC)

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
# The Client build wants the shared libraries in $(bindir),
# so we also install them there.
#

export:: $(TARGETS)
	$(INSTALL) -m 444 $(TARGETS) $(libdir)
ifdef SHARED_LIBRARY
ifeq ($(OS_ARCH),HP-UX)
	$(INSTALL) -m 755 $(SHARED_LIBRARY) $(libdir)
	$(INSTALL) -m 755 $(SHARED_LIBRARY) $(bindir)
else
	$(INSTALL) -m 444 $(SHARED_LIBRARY) $(bindir)
endif
ifeq ($(OS_ARCH), OpenVMS)
	$(INSTALL) -m 444 $(SHARED_LIBRARY:.$(DLL_SUFFIX)=.vms) $(libdir)
endif
endif
ifeq ($(MOZ_BITS),16)
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/lib
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/bin
endif

install:: export
