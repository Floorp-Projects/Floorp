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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
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

#
# Configuration common to all versions of Windows NT
# and Windows 95
#

DEFAULT_COMPILER = cl

ifdef NS_USE_GCC
	CC           = gcc
	CCC          = g++
	LINK         = ld
	AR           = ar
	AR          += cr $@
	RANLIB       = ranlib
	BSDECHO      = echo
	RC           = windres.exe -O coff --use-temp-file
	LINK_DLL      = $(CC) $(OS_DLLFLAGS) $(DLLFLAGS)
else
	CC           = cl
	CCC          = cl
	LINK         = link
	AR           = lib
	AR          += -NOLOGO -OUT:"$@"
	RANLIB       = echo
	BSDECHO      = echo
	RC           = rc.exe
endif

ifdef BUILD_TREE
NSINSTALL_DIR  = $(BUILD_TREE)/nss
else
NSINSTALL_DIR  = $(CORE_DEPTH)/coreconf/nsinstall
endif
NSINSTALL      = nsinstall

MKDEPEND_DIR    = $(CORE_DEPTH)/coreconf/mkdepend
MKDEPEND        = $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend.exe
# Note: MKDEPENDENCIES __MUST__ be a relative pathname, not absolute.
# If it is absolute, gmake will crash unless the named file exists.
MKDEPENDENCIES  = $(OBJDIR_NAME)/depend.mk

INSTALL      = $(NSINSTALL)
MAKE_OBJDIR  = mkdir
MAKE_OBJDIR += $(OBJDIR)
GARBAGE     += $(OBJDIR)/vc20.pdb $(OBJDIR)/vc40.pdb
XP_DEFINE   += -DXP_PC
ifdef NS_USE_GCC
LIB_SUFFIX   = a
else
LIB_SUFFIX   = lib
endif
DLL_SUFFIX   = dll

ifdef NS_USE_GCC
    OS_CFLAGS += -mno-cygwin -mms-bitfields
    _GEN_IMPORT_LIB=-Wl,--out-implib,$(IMPORT_LIBRARY)
    DLLFLAGS  += -mno-cygwin -o $@ -shared -Wl,--export-all-symbols $(if $(IMPORT_LIBRARY),$(_GEN_IMPORT_LIB))
    ifdef BUILD_OPT
	OPTIMIZER  += -O2
	DEFINES    += -UDEBUG -U_DEBUG -DNDEBUG
	#
	# Add symbolic information for a profiler
	#
	ifdef MOZ_PROFILE
		OPTIMIZER += -g
	endif
    else
	OPTIMIZER  += -g
	NULLSTRING :=
	SPACE      := $(NULLSTRING) # end of the line
	USERNAME   := $(subst $(SPACE),_,$(USERNAME))
	USERNAME   := $(subst -,_,$(USERNAME))
	DEFINES    += -DDEBUG -D_DEBUG -UNDEBUG -DDEBUG_$(USERNAME)
    endif
else # !NS_USE_GCC
    ifdef BUILD_OPT
	OS_CFLAGS  += -MD
	OPTIMIZER  += -O2
	DEFINES    += -UDEBUG -U_DEBUG -DNDEBUG
	DLLFLAGS   += -OUT:"$@"
	#
	# Add symbolic information for a profiler
	#
	ifdef MOZ_PROFILE
		OPTIMIZER += -Z7
	endif
	ifdef MOZ_DEBUG_SYMBOLS
		OPTIMIZER += -Zi
	endif
	ifneq (,$(MOZ_PROFILE)$(MOZ_DEBUG_SYMBOLS))
		DLLFLAGS += -DEBUG -OPT:REF
	endif
    else
	#
	# Define USE_DEBUG_RTL if you want to use the debug runtime library
	# (RTL) in the debug build
	#
	ifdef USE_DEBUG_RTL
		OS_CFLAGS += -MDd
	else
		OS_CFLAGS += -MD
	endif
	OPTIMIZER  += -Od -Z7
	#OPTIMIZER += -Zi -Fd$(OBJDIR)/ -Od
	NULLSTRING :=
	SPACE      := $(NULLSTRING) # end of the line
	USERNAME   := $(subst $(SPACE),_,$(USERNAME))
	USERNAME   := $(subst -,_,$(USERNAME))
	DEFINES    += -DDEBUG -D_DEBUG -UNDEBUG -DDEBUG_$(USERNAME)
	DLLFLAGS   += -DEBUG -OUT:"$@"
	# Purify requires /FIXED:NO when linking EXEs.
	LDFLAGS    += -DEBUG -PDB:NONE /FIXED:NO
    endif
endif # NS_USE_GCC

DEFINES += -DWIN32
ifdef MAPFILE
ifndef NS_USE_GCC
DLLFLAGS += -DEF:$(MAPFILE)
endif
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $(LIBRARY_NAME).def $@


#
#  The following is NOT needed for the NSPR 2.0 library.
#

DEFINES += -D_WINDOWS

# override default, which is ASFLAGS = CFLAGS
ifdef NS_USE_GCC
	AS	= $(CC)
	ASFLAGS = $(INCLUDES)
else
	AS	= ml.exe
	ASFLAGS = -Cp -Sn -Zi -coff $(INCLUDES)
endif

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
# override the definitions of IMPORT_LIB_PREFIX, LIB_PREFIX, and
# DLL_PREFIX in prefix.mk
#

ifndef IMPORT_LIB_PREFIX
    ifdef NS_USE_GCC
	IMPORT_LIB_PREFIX = lib
    else
	IMPORT_LIB_PREFIX = $(NULL)
    endif
endif

ifndef LIB_PREFIX
    ifdef NS_USE_GCC
	LIB_PREFIX = lib
    else
	LIB_PREFIX = $(NULL)
    endif
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
    ifdef NS_USE_GCC
	OBJ_SUFFIX = .o
    else
	OBJ_SUFFIX = .obj
    endif
endif

#
# Assembler source suffixes
#
ifndef ASM_SUFFIX
    ifdef NS_USE_GCC
	ASM_SUFFIX = .s
    else
	ASM_SUFFIX = .asm
    endif
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

