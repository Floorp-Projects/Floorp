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

OBJS = $(OBJDIR)/mygetval.$(OBJ_SUFFIX) \
	$(OBJDIR)/mysetval.$(OBJ_SUFFIX)

ifeq ($(OS_TARGET), WIN16)
W16OBJS = $(subst $(space),$(comma)$(space),$(OBJS))
endif

ifeq ($(OS_ARCH), WINNT)
ifeq (,$(filter-out WIN16 OS2,$(OS_TARGET)))
# do nothing
else
DLLBASE=/BASE:0x30000000
RES=$(OBJDIR)/my.res
RESNAME=../../../pr/src/nspr.rc
endif
endif

ifeq ($(OS_ARCH), WINNT)
IMPORT_LIBRARY	= $(OBJDIR)/my.lib
SHARED_LIBRARY	= $(OBJDIR)/my.dll
ifeq ($(OS_TARGET), OS2)
LIBRARY		= $(OBJDIR)/my_s.lib
# XXX $(LIBRARY) probably doesn't need to be added to TARGETS.
TARGETS		= $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY)
else
TARGETS		= $(SHARED_LIBRARY) $(IMPORT_LIBRARY)
endif
else
ifdef MKSHLIB
SHARED_LIBRARY	= $(OBJDIR)/libmy.$(DLL_SUFFIX)
endif
TARGETS		= $(SHARED_LIBRARY)
endif

#
# To create a loadable module on Rhapsody, we must override
# -dynamiclib with -bundle.
#
ifeq ($(OS_ARCH),Rhapsody)
DSO_LDOPTS = -bundle
endif

include $(MOD_DEPTH)/config/rules.mk

ifeq ($(OS_TARGET), WIN16)
# Note: The Win16 target: my.dll requires these macros
# to be overridden to build the test .dll
# default values in win16...mk are for release targets.
#
OS_DLL_OPTION = NOCASEEXACT
OS_LIB_FLAGS = -irn
endif

ifdef SHARED_LIBRARY
export:: $(TARGETS)
	$(NSINSTALL) $(TARGETS) ../$(OBJDIR)/dll

install:: export

clean::
	rm -rf $(TARGETS)
endif
