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

######################################################################
# Config stuff for BeOS (all architectures)
######################################################################

######################################################################
# Version-independent
######################################################################

DEFINES			+= 
XP_DEFINE		= -DXP_BEOS

OBJ_SUFFIX              = o
LIB_SUFFIX              = a
DLL_SUFFIX              = so
AR                      = ar cr $@

ifdef BUILD_OPT
DEFINES                 = -UDEBUG -DNDEBUG
OBJDIR_TAG              = _OPT
else
DEFINES                 = -DDEBUG -UNDEBUG
OBJDIR_TAG              = _DBG
endif

ifeq (PC,$(findstring PC,$(OS_TEST)))
CPU_ARCH		= x86
CC                      = gcc
CCC                     = g++
LD                      = gcc
RANLIB                  = ranlib
DSO_LDOPTS              = -nostart
PORT_FLAGS		= -DHAVE_STRERROR
ifdef BUILD_OPT
OPTIMIZER		= -O2
LDFLAGS			+= -s
else
OPTIMIZER		= -gdwarf-2 -O0
endif
else
CPU_ARCH		= ppc
CC                      = mwcc
CCC                     = mwcc
LD                      = mwld
RANLIB                  = ranlib
DSO_LDOPTS              = -xms -export pragma \
					-init _init_routine_ \
					-term _term_routine_ \
					-lroot -lnet \
					/boot/develop/lib/ppc/glue-noinit.a \
					/boot/develop/lib/ppc/init_term_dyn.o \
					/boot/develop/lib/ppc/start_dyn.o 

PORT_FLAGS		= -DHAVE_STRERROR -D_POSIX_SOURCE
ifdef BUILD_OPT
OPTIMIZER		= -O2
else
OPTIMIZER		= -g -O0
endif
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

OS_INCLUDES		=  -I- -I. 
#G++INCLUDES		= -I/usr/include/g++

PLATFORM_FLAGS		= -DBeOS -DBEOS $(OS_INCLUDES)

OS_CFLAGS		= $(DSO_CFLAGS) $(PLATFORM_FLAGS) $(PORT_FLAGS)

USE_BTHREADS = 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

OBJDIR_NAME	= $(OS_CONFIG)_$(CPU_ARCH)$(OBJDIR_TAG).OBJ

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

NSINSTALL       = $(MOD_DEPTH)/config/$(OBJDIR_NAME)/nsinstall

ifeq ($(NSDISTMODE),copy)
# copy files, but preserve source mtime
INSTALL         = $(NSINSTALL) -t
else
ifeq ($(NSDISTMODE),absolute_symlink)
# install using absolute symbolic links
INSTALL         = $(NSINSTALL) -L `$(NFSPWD)`
else
# install using relative symbolic links
INSTALL         = $(NSINSTALL) -R
endif
endif

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
endef
