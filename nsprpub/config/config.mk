#! gmake
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
