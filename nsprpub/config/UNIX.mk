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

PR_UNIXOS	= 1
XP_DEFINE	= -DXP_UNIX
OBJ_SUFFIX	= o
LIB_SUFFIX	= a
DLL_SUFFIX	= so
AR		= ar cr $@

ifdef BUILD_OPT
OPTIMIZER	= -O
DEFINES		= -UDEBUG -DNDEBUG
OBJDIR_TAG	= _OPT
else
OPTIMIZER	= -g
DEFINES		= -DDEBUG -UNDEBUG -DDEBUG_$(shell whoami)
OBJDIR_TAG	= _DBG
endif

# Name of the binary code directories
ifdef MOZILLA_CLIENT
OBJDIR_NAME	= $(OS_CONFIG)$(CPU_ARCH_TAG)$(OBJDIR_TAG).OBJ
else
OBJDIR_NAME	= $(OS_CONFIG)$(CPU_ARCH_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJ
endif

MKDEPEND_DIR    = $(DEPTH)/config/mkdepend
MKDEPEND 	= $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES  = $(OBJDIR)/depend.mk

####################################################################
#
# One can define the makefile variable NSDISTMODE to control
# how files are published to the 'dist' directory.  If not
# defined, the default is "install using relative symbolic
# links".  The two possible values are "copy", which copies files
# but preserves source mtime, and "absolute_symlink", which
# installs using absolute symbolic links.  The "absolute_symlink"
# option requires NFSPWD.
#
####################################################################

NSINSTALL	= $(MOD_DEPTH)/config/$(OBJDIR_NAME)/nsinstall

ifeq ($(NSDISTMODE),copy)
# copy files, but preserve source mtime
INSTALL		= $(NSINSTALL) -t
else
ifeq ($(NSDISTMODE),absolute_symlink)
# install using absolute symbolic links
INSTALL		= $(NSINSTALL) -L `$(NFSPWD)`
else
# install using relative symbolic links
INSTALL		= $(NSINSTALL) -R
endif
endif

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); else true; fi
endef
