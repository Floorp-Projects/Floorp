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

# Configuration information for building in the NSPR source module

# Define an include-at-most-once-flag
NSPR_CONFIG_MK	= 1

#
# The variable definitions in this file are inputs to NSPR's
# build system.  This file, if present, is included at the
# beginning of config.mk.
#
# For example:
#
# BUILD_OPT=1
# USE_PTHREADS=1
# NS_USE_GCC=
#
ifndef NSPR_MY_CONFIG_MK
NSPR_MY_CONFIG_MK = $(MOD_DEPTH)/config/my_config.mk
endif

#
# The variable definitions in this file are used to
# override variable values set by NSPR's build system.
# This file, if present, is included at the end of config.mk.
#
# For example:
#
# DIST=/usr/local/nspr
#
ifndef NSPR_MY_OVERRIDES_MK
NSPR_MY_OVERRIDES_MK = $(MOD_DEPTH)/config/my_overrides.mk
endif

-include $(NSPR_MY_CONFIG_MK)

include $(MOD_DEPTH)/config/module.df

include $(MOD_DEPTH)/config/arch.mk

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

CFLAGS		= $(CC_ONLY_FLAGS) $(OPTIMIZER) $(OS_CFLAGS)\
		  $(XP_DEFINE) $(DEFINES) $(INCLUDES) $(XCFLAGS)
CCCFLAGS	= $(CCC_ONLY_FLAGS) $(OPTIMIZER) $(OS_CFLAGS)\
		  $(XP_DEFINE) $(DEFINES) $(INCLUDES) $(XCFLAGS)
# For purify
NOMD_CFLAGS	= $(CC_ONLY_FLAGS) $(OPTIMIZER) $(NOMD_OS_CFLAGS)\
		  $(XP_DEFINE) $(DEFINES) $(INCLUDES) $(XCFLAGS)

include $(MOD_DEPTH)/config/$(OS_TARGET).mk

# Figure out where the binary code lives.
BUILD		= $(OBJDIR_NAME)
OBJDIR		= $(OBJDIR_NAME)
DIST		= $(NSDEPTH)/dist/$(OBJDIR_NAME)
ifeq ($(MOZ_BITS),16)
MOZ_INCL	= $(NSDEPTH)/dist/public/win16
MOZ_DIST	= $(NSDEPTH)/dist/WIN16D_D.OBJ
endif

VPATH		= $(OBJDIR)
DEPENDENCIES	= $(OBJDIR)/.md

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
RELEASE_BIN_DIR = $(RELEASE_DIR)/$(BUILD_NUMBER)/$(OBJDIR_NAME)/bin
RELEASE_LIB_DIR = $(RELEASE_DIR)/$(BUILD_NUMBER)/$(OBJDIR_NAME)/lib

-include $(NSPR_MY_OVERRIDES_MK)
