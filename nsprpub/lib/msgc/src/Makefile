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

INCLUDES = -I$(DIST)/include -I../include

CSRCS = prgcapi.c prmsgc.c

ifeq ($(OS_ARCH),WINNT)
ifeq ($(OS_TARGET),WIN16)
CSRCS += win16gc.c
else
CSRCS += win32gc.c
endif
else
ifeq ($(OS_ARCH),OS2)
CSRCS += os2gc.c
else
CSRCS += unixgc.c
endif
endif

NSPR_VERSION = $(MOD_VERSION)

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
EXTRA_LIBS = $(DIST)/lib/nspr$(NSPR_VERSION).lib
else
DLLBASE=/BASE:0x30000000
#RES=$(OBJDIR)/ds.res
#RESNAME=$(MOD_DEPTH)/pr/src/nspr.rc
#OS_LIBS = user32.lib
EXTRA_LIBS = $(DIST)/lib/libnspr$(NSPR_VERSION).lib

ifdef MOZ_DEBUG
ifdef GLOWCODE
EXTRA_LIBS += $(GLOWDIR)/glowcode.lib
endif
endif
endif

else
ifeq ($(MOZ_OS2_TOOLS),VACPP)
EXTRA_LIBS = $(DIST)/lib/nspr$(NSPR_VERSION).$(LIB_SUFFIX)

else
ifeq ($(OS_ARCH), AIX)
ifeq ($(CLASSIC_NSPR),1)
OS_LIBS += -lc
else
OS_LIBS += -lc_r
endif
endif
ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
EXTRA_LIBS = -L$(DIST)/lib -lnspr$(NSPR_VERSION)_shr
else
EXTRA_LIBS = -L$(DIST)/lib -lnspr$(NSPR_VERSION)
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

LIBRARY_NAME = msgc
LIBRARY_VERSION = $(MOD_VERSION)

RELEASE_LIBS = $(TARGETS)

include $(MOD_DEPTH)/config/rules.mk

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
