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

#
# Configuration common to all (supported) versions of OS/2
#
# OS_CFLAGS is the command line options for the compiler when
#   building the .DLL object files.
# OS_EXE_CFLAGS is the command line options for the compiler
#   when building the .EXE object files; this is for the test
#   programs.
# the macro OS_CFLAGS is set to OS_EXE_CFLAGS inside of the
#   makefile for the pr/tests directory. ... Hack.


#
# On OS/2 we proudly support gbash...
#
SHELL = GBASH.EXE

CC			= icc -q -DXP_OS2 -N10
CCC			= icc -q -DXP_OS2 -DOS2=4 -N10
LINK			= flipper ilink
AR			= flipper ilibo //noignorecase //nologo $@
RANLIB = echo
BSDECHO = echo
NSINSTALL = nsinstall
INSTALL	= $(NSINSTALL)
MAKE_OBJDIR = mkdir $(OBJDIR)
IMPLIB = flipper implib -nologo -noignorecase
FILTER = flipper cppfilt -q
RC = rc.exe

GARBAGE =

XP_DEFINE = -DXP_PC
OBJ_SUFFIX = obj
LIB_SUFFIX = lib
DLL_SUFFIX = dll

OS_CFLAGS     = -I. -W3 -gm -gd+ -sd- -su4 -ge-
OS_EXE_CFLAGS = -I. -W3 -gm -gd+ -sd- -su4 
AR_EXTRA_ARGS = ,,

ifdef BUILD_OPT
OPTIMIZER	= -O+ -Oi 
DEFINES = -UDEBUG -U_DEBUG -DNDEBUG
DLLFLAGS	= -DLL -OUT:$@ -MAP:$(@:.dll=.map)
OBJDIR_TAG = _OPT
else
OPTIMIZER	= -Ti+
DEFINES = -DDEBUG -D_DEBUG -UNDEBUG
DLLFLAGS	= -DEBUG -DLL -OUT:$@ -MAP:$(@:.dll=.map)
OBJDIR_TAG = _DBG
LDFLAGS = -DEBUG 
endif

DEFINES += -DOS2=4 -DBSD_SELECT
DEFINES += -D_X86_
DEFINES += -D_PR_GLOBAL_THREADS_ONLY

# Name of the binary code directories
ifeq ($(CPU_ARCH),x386)
ifdef MOZ_LITE
OBJDIR_NAME = $(subst OS2,NAV,$(OS_CONFIG))$(OBJDIR_TAG).OBJ
else
OBJDIR_NAME = $(OS_CONFIG)$(OBJDIR_TAG).OBJ
endif
else
OBJDIR_NAME = $(OS_CONFIG)$(CPU_ARCH)$(OBJDIR_TAG).OBJ
endif

OS_DLLFLAGS = -nologo -DLL -FREE -NOE
