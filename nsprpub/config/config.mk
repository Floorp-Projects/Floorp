#! gmake
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

ifndef topsrcdir
topsrcdir = $(MOD_DEPTH)
endif

# Configuration information for building in the NSPR source module

# Define an include-at-most-once-flag
NSPR_CONFIG_MK	= 1

include $(topsrcdir)/config/module.df

include $(topsrcdir)/config/arch.mk

ifndef NSDEPTH
NSDEPTH = $(MOD_DEPTH)/..
endif

#
# Default command macros; can be overridden in <arch>.mk.
#
# XXX FIXME: I removed CCF and LINKEXE.
AS		= $(CC)
ASFLAGS		= $(CFLAGS)
PURIFY		= purify $(PURIFYOPTIONS)
LINK_DLL	= $(LINK) $(OS_DLLFLAGS) $(DLLFLAGS)
NFSPWD		= $(MOD_DEPTH)/config/nfspwd

LIBNSPR		= $(DIST)/lib/libnspr.$(LIB_SUFFIX)
PURELIBNSPR	= $(DIST)/lib/libpurenspr.$(LIB_SUFFIX)

CFLAGS		= $(OPTIMIZER) $(OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) \
				$(XCFLAGS)
# For purify
NOMD_CFLAGS	= $(OPTIMIZER) $(NOMD_OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) \
				$(XCFLAGS)

ifdef USE_AUTOCONF
OPTIMIZER	= $(ACCFLAGS)
DEFINES		+=  -UDEBUG -DNDEBUG -DTRIMMED
endif

ifdef MOZ_DEBUG
OPTIMIZER	= -g
JAVA_OPTIMIZER	= -g
DEFINES		= -DDEBUG -UNDEBUG -DDEBUG_$(shell $(WHOAMI)) -DTRACING
XBCFLAGS	= -FR$*
endif

include $(topsrcdir)/config/$(OS_TARGET).mk

# Figure out where the binary code lives.
BUILD		= $(OBJDIR_NAME)
OBJDIR		= $(OBJDIR_NAME)
DIST		= $(NSDEPTH)/dist/$(OBJDIR_NAME)
ifeq ($(MOZ_BITS),16)
MOZ_INCL	= $(NSDEPTH)/dist/public/win16
MOZ_DIST	= $(NSDEPTH)/dist/WIN16D_D.OBJ
endif

ifdef USE_AUTOCONF
EMACS			= $(ACEMACS)
PERL			= $(ACPERL)
RANLIB			= $(ACRANLIB)
UNZIP_PROG		= $(ACUNZIP)
WHOAMI			= $(ACWHOAMI)
ZIP_PROG		= $(ACZIP)
else
VPATH		= $(OBJDIR)
DEPENDENCIES	= $(OBJDIR)/.md
endif

ifdef BUILD_DEBUG_GC
DEFINES		+= -DDEBUG_GC
endif

GARBAGE		+= $(DEPENDENCIES) core $(wildcard core.[0-9]*)

####################################################################
#
# The NSPR-specific configuration
#
####################################################################

OS_CFLAGS += -DFORCE_PR_LOG

ifeq ($(_PR_NO_CLOCK_TIMER),1)
OS_CFLAGS += -D_PR_NO_CLOCK_TIMER
endif

ifeq ($(USE_PTHREADS), 1)
OS_CFLAGS += -D_PR_PTHREADS -UHAVE_CVAR_BUILT_ON_SEM
endif

ifeq ($(PTHREADS_USER), 1)
OS_CFLAGS += -DPTHREADS_USER -UHAVE_CVAR_BUILT_ON_SEM
endif

ifeq ($(USE_IPV6),1)
OS_CFLAGS += -D_PR_INET6
endif

####################################################################
#
# Configuration for the release process
#
####################################################################

MDIST = /m/dist
ifeq ($(OS_ARCH),WINNT)
MDIST = //helium/dist
MDIST_DOS = \\\\helium\\dist
endif

# RELEASE_DIR is ns/dist/<module name>

RELEASE_DIR = $(NSDEPTH)/dist/release/$(MOD_NAME)

RELEASE_INCLUDE_DIR = $(RELEASE_DIR)/$(BUILD_NUMBER)/$(OBJDIR_NAME)/include
RELEASE_LIB_DIR = $(RELEASE_DIR)/$(BUILD_NUMBER)/$(OBJDIR_NAME)/lib
