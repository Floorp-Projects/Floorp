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

#
# win16_3.11.mk -- Make configuration for Win16
#
# This file configures gmake to build the Win16 variant of
# NSPR 2.0. This file has the function of two files commonly
# used on other platforms, for example: winnt.mk and
# winnt4.0.mk. ... The packaging is easier and there is only
# one variant of the Win16 target.
# 
# Win16 is built using the Watcom C/C++ version 11.0
# compiler. You gotta set up the compiler first. Follow the
# directions in the manual (Ha! ... really, its not a
# problem). The Watcom compiler depends on a few environment
# variables; these environment variables define where the
# compiler components are installed; they must be set before
# running the make.
# 
# Notes:
# OS_CFLAGS is the command line options for the compiler when
#   building the .DLL object files.
# OS_EXE_CFLAGS is the command line options for the compiler
#   when building the .EXE object files; this is for the test
#   programs.
# the macro OS_CFLAGS is set to OS_EXE_CFLAGS inside of the
#   makefile for the pr/tests directory. ... Hack.
# 
# USE_WATCOM_DEBUG_DATA environment variable causes the
#   watcom compiler flag to be set to -hw (otherwise
#   it is set to -hc (codeview debug data)) for debug builds.
#

# -- configuration -----------------------------------------

CC = wcc
CCC = wcl
LINK = wlink
AR = wlib -q $@
RC = wrc.exe /r /dWIN16=1 /bt=windows
RANLIB = echo
BSDECHO = echo
NSINSTALL = nsinstall
INSTALL	= $(NSINSTALL)
MAKE_OBJDIR = mkdir $(OBJDIR)

XP_DEFINE = -DXP_PC
OBJ_SUFFIX = obj
LIB_SUFFIX = lib
DLL_SUFFIX = dll

ifdef BUILD_OPT
OBJDIR_TAG = _O
OPTIMIZER = -oneatx -oh -oi -ei -3 -fpi87 -fp3 -s
else
ifdef USE_WATCOM_DEBUG_DATA
OPTIMIZER = -d2 -hw -s -DDEBUG
DEBUGTYPE = watcom
else
OPTIMIZER = -d2 -hc -s -DDEBUG
DEBUGTYPE = codeview
endif
OBJDIR_TAG = _D
endif

# XXX FIXME: I doubt we use this.  It is redundant with
# SHARED_LIBRARY.
ifdef DLL
DLL := $(addprefix $(OBJDIR)/, $(DLL))
endif


CPU_ARCH = x86
OS_CFLAGS = -ml -3 -bd -zc -zu -bt=windows -d_X86_ -dWIN16 -d_WINDLL
OS_EXE_CFLAGS = -ml -3 -bt=windows -d_X86_ -dWIN16
OS_LIB_FLAGS = -c -iro -n

# Name of the binary code directories
OBJDIR_NAME     = $(OS_CONFIG)$(OBJDIR_TAG).OBJ

OS_DLL_OPTION = CASEEXACT
OS_DLLFLAGS =
OS_LIBS =
W16_EXPORTS = #
