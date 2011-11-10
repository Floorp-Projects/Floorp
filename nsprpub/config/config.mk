#! gmake
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape Portable Runtime (NSPR).
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
# MOZ_OPTIMIZE=1
# USE_PTHREADS=1
# NS_USE_GCC=
#
ifndef topsrcdir
topsrcdir=$(MOD_DEPTH)
endif

ifndef srcdir
srcdir=.
endif

NFSPWD		= $(MOD_DEPTH)/config/nfspwd

CFLAGS		= $(VISIBILITY_FLAGS) $(CC_ONLY_FLAGS) $(OPTIMIZER)\
		  $(OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) $(XCFLAGS)
CCCFLAGS	= $(VISIBILITY_FLAGS) $(CCC_ONLY_FLAGS) $(OPTIMIZER)\
		  $(OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) $(XCFLAGS)
# For purify
NOMD_CFLAGS	= $(CC_ONLY_FLAGS) $(OPTIMIZER) $(NOMD_OS_CFLAGS)\
		  $(XP_DEFINE) $(DEFINES) $(INCLUDES) $(XCFLAGS)
NOMD_CCFLAGS	= $(CCC_ONLY_FLAGS) $(OPTIMIZER) $(NOMD_OS_CFLAGS)\
		  $(XP_DEFINE) $(DEFINES) $(INCLUDES) $(XCFLAGS)

LDFLAGS		= $(OS_LDFLAGS)

# Enable profile-guided optimization
ifndef NO_PROFILE_GUIDED_OPTIMIZE
ifdef MOZ_PROFILE_GENERATE
CFLAGS += $(PROFILE_GEN_CFLAGS)
LDFLAGS += $(PROFILE_GEN_LDFLAGS)
DLLFLAGS += $(PROFILE_GEN_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_GENERATE

ifdef MOZ_PROFILE_USE
CFLAGS += $(PROFILE_USE_CFLAGS)
LDFLAGS += $(PROFILE_USE_LDFLAGS)
DLLFLAGS += $(PROFILE_USE_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_USE
endif # NO_PROFILE_GUIDED_OPTIMIZE

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
endef

LINK_DLL	= $(LD) $(OS_DLLFLAGS) $(DLLFLAGS)

ifeq ($(OS_ARCH),Darwin)
PWD := $(shell pwd)
endif

ifeq (,$(CROSS_COMPILE)$(filter-out WINNT OS2, $(OS_ARCH)))
INSTALL		= $(NSINSTALL)
else
ifeq ($(NSDISTMODE),copy)
# copy files, but preserve source mtime
INSTALL		= $(NSINSTALL) -t
else
ifeq ($(NSDISTMODE),absolute_symlink)
# install using absolute symbolic links
ifeq ($(OS_ARCH),Darwin)
INSTALL		= $(NSINSTALL) -L $(PWD)
else
INSTALL		= $(NSINSTALL) -L `$(NFSPWD)`
endif
else
# install using relative symbolic links
INSTALL		= $(NSINSTALL) -R
endif
endif
endif # (WINNT || OS2) && !CROSS_COMPILE

DEPENDENCIES	= $(OBJDIR)/.md

ifdef BUILD_DEBUG_GC
DEFINES		+= -DDEBUG_GC
endif

GARBAGE		+= $(DEPENDENCIES) core $(wildcard core.[0-9]*)

DIST_GARBAGE += Makefile

####################################################################
#
# The NSPR-specific configuration
#
####################################################################

DEFINES += -DFORCE_PR_LOG

ifeq ($(_PR_NO_CLOCK_TIMER),1)
DEFINES += -D_PR_NO_CLOCK_TIMER
endif

ifeq ($(USE_PTHREADS), 1)
DEFINES += -D_PR_PTHREADS -UHAVE_CVAR_BUILT_ON_SEM
endif

ifeq ($(PTHREADS_USER), 1)
DEFINES += -DPTHREADS_USER -UHAVE_CVAR_BUILT_ON_SEM
endif

ifeq ($(USE_IPV6),1)
DEFINES += -D_PR_INET6
endif

ifeq ($(MOZ_UNICODE),1)
DEFINES += -DMOZ_UNICODE
endif

####################################################################
#
# Configuration for the release process
#
####################################################################

MDIST = /m/dist
ifeq ($(OS_ARCH),WINNT)
MDIST = //helium/dist
MDIST_DOS = $(subst /,\\,$(MDIST))
endif

# RELEASE_DIR is ns/dist/<module name>

RELEASE_DIR = $(MOD_DEPTH)/dist/release/$(MOD_NAME)

RELEASE_INCLUDE_DIR = $(RELEASE_DIR)/$(BUILD_NUMBER)/$(OBJDIR_NAME)/include
RELEASE_BIN_DIR = $(RELEASE_DIR)/$(BUILD_NUMBER)/$(OBJDIR_NAME)/bin
RELEASE_LIB_DIR = $(RELEASE_DIR)/$(BUILD_NUMBER)/$(OBJDIR_NAME)/lib

# autoconf.mk sets OBJ_SUFFIX to an error to avoid use before including
# this file
OBJ_SUFFIX := $(_OBJ_SUFFIX)

# PGO builds with GCC build objects with instrumentation in a first pass,
# then objects optimized, without instrumentation, in a second pass. If
# we overwrite the ojects from the first pass with those from the second,
# we end up not getting instrumentation data for better optimization on
# incremental builds. As a consequence, we use a different object suffix
# for the first pass.
ifdef MOZ_PROFILE_GENERATE
ifdef NS_USE_GCC
OBJ_SUFFIX := i_o
endif
endif
