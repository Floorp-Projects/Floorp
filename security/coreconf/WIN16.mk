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

#
# override the definitions of RELEASE_TREE found in tree.mk
#
ifndef RELEASE_TREE
    ifdef BUILD_SHIP
	ifdef USE_SHIPS
	    RELEASE_TREE = $(NTBUILD_SHIP)
	else
	    RELEASE_TREE = //redbuild/components
	endif
    else
	RELEASE_TREE = //redbuild/components
    endif
endif

#
# override the definitions of LIB_PREFIX and DLL_PREFIX in prefix.mk
#
ifndef LIB_PREFIX
    LIB_PREFIX =  $(NULL)
endif

ifndef DLL_PREFIX
    DLL_PREFIX =  $(NULL)
endif

#
# override the definitions of various _SUFFIX symbols in suffix.mk
#

#
# Object suffixes
#
ifndef OBJ_SUFFIX
    OBJ_SUFFIX = .obj
endif

#
# Assembler source suffixes
#
ifndef ASM_SUFFIX
    ASM_SUFFIX = .asm
endif

#
# Library suffixes
#
ifndef IMPORT_LIB_SUFFIX
    IMPORT_LIB_SUFFIX = .$(LIB_SUFFIX)
endif

ifndef DYNAMIC_LIB_SUFFIX_FOR_LINKING
    DYNAMIC_LIB_SUFFIX_FOR_LINKING = $(IMPORT_LIB_SUFFIX)
endif

#
# Program suffixes
#
ifndef PROG_SUFFIX
    PROG_SUFFIX = .exe
endif

#
# When the processor is NOT 386-based on Windows NT, override the
# value of $(CPU_TAG).  For WinNT, 95, 16, not CE.
#
ifneq ($(CPU_ARCH),x386)
    CPU_TAG = _$(CPU_ARCH)
endif

#
# override ruleset.mk, removing the "lib" prefix for library names, and
# adding the "32" after the LIBRARY_VERSION.
#
ifdef LIBRARY_NAME
    SHARED_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)32$(JDK_DEBUG_SUFFIX).dll
    IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)32$(JDK_DEBUG_SUFFIX).lib
endif

#
# override the TARGETS defined in ruleset.mk, adding IMPORT_LIBRARY
#
ifndef TARGETS
    TARGETS = $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(PROGRAM)
endif
