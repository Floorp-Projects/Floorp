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
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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
# compiler. You gotta set up the compiler first. 
# The Watcom compiler depends on a few environment
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
# 
# 
# 

# -- configuration -----------------------------------------

DEFAULT_COMPILER = wcc

CC           = wcc
CCC          = wcl
LINK         = wlink
AR           = wlib
AR          += -q $@
RC           = wrc.exe
RC          += /r /dWIN16=1 /bt=windows
RANLIB       = echo
BSDECHO      = echo
NSINSTALL_DIR  = $(CORE_DEPTH)/coreconf/nsinstall
NSINSTALL      = nsinstall
INSTALL	     = $(NSINSTALL)
MAKE_OBJDIR  = mkdir
MAKE_OBJDIR += $(OBJDIR)
XP_DEFINE   += -DXP_PC
LIB_SUFFIX   = lib
DLL_SUFFIX   = dll

ifdef BUILD_OPT
	OPTIMIZER   = -oneatx -oh -oi -ei -3 -fpi87 -fp3
else
	OPTIMIZER  += -d2 -hc -DDEBUG
#	OPTIMIZER  += -d2 -hw -DDEBUG
#	LDFLAGS  += -DEBUG -DEBUGTYPE:CV
endif

#
# $(CPU_ARCH) has been commented out so that its contents
# are not added to the WIN16_?.OBJ names thus expanding
# them beyond the 8.3 character limit for this platform.
#
#CPU_ARCH       = x386
#
# added "-s" to avoid dependency on watcom's libs (e.g. on _STK)
# added "-zt3" for compatibility with MSVC's "/Gt3" option
#
OS_CFLAGS     += -ml -3 -bd -zc -zu -bt=windows -s -zt3 -d_X86_ -dWIN16 -d_WINDLL 
#OS_EXE_CFLAGS += -ml -3 -bt=windows -d_X86_ -dWIN16
OS_LIB_FLAGS   = -c -iro

# Name of the binary code directories
OS_DLL_OPTION = CASEEXACT
OS_DLLFLAGS  =
OS_LIBS      =
W16_EXPORTS  = #
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = copy $(LIBRARY_NAME).def $@


#
#  The following is NOT needed for the NSPR 2.0 library.
#

OS_CFLAGS += -d_WINDOWS -d_MSC_VER=700
