#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Configuration common to all versions of Windows CE and Pocket PC x.
#

ifeq ($(CPU_ARCH),x86)
    DEFAULT_COMPILER = cl
    CC           = cl
    CCC          = cl
else
ifeq ($(CPU_ARCH),ARM)
    DEFAULT_COMPILER = clarm
    CC           = clarm
    CCC          = clarm
else
include CPU_ARCH_is_not_recognized
include _$(CPU_ARCH)
endif
endif

LINK         = link
AR           = lib
AR          += -NOLOGO -OUT:"$@"
RANLIB       = echo
BSDECHO      = echo

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
RC           = rc.exe
GARBAGE     += $(OBJDIR)/vc20.pdb $(OBJDIR)/vc40.pdb
XP_DEFINE   += -DXP_PC
LIB_SUFFIX   = lib
DLL_SUFFIX   = dll
OS_DLLFLAGS  += -DLL

EXTRA_EXE_LD_FLAGS += -ENTRY:mainWCRTStartup

ifdef BUILD_OPT
#   OS_CFLAGS  += -MD
    OPTIMIZER  += -O2
    DEFINES    += -UDEBUG -U_DEBUG -DNDEBUG
    DLLFLAGS   += -OUT:"$@"
else
    #
    # Define USE_DEBUG_RTL if you want to use the debug runtime library
    # (RTL) in the debug build
    #
    ifdef USE_DEBUG_RTL
#	OS_CFLAGS += -MDd
    else
#	OS_CFLAGS += -MD
    endif
    OPTIMIZER  += -Od -Z7
    #OPTIMIZER += -Zi -Fd$(OBJDIR)/ -Od
    DEFINES    += -DDEBUG -D_DEBUG -UNDEBUG -DDEBUG_$(USERNAME)
    DLLFLAGS   += -DEBUG -DEBUGTYPE:CV -OUT:"$@"
    LDFLAGS    += -DEBUG -DEBUGTYPE:CV
endif

# DEFINES += -DWIN32

ifdef MAPFILE
    DLLFLAGS += -DEF:$(MAPFILE)
endif

# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@

#
#  The following is NOT needed for the NSPR 2.0 library.
#

DEFINES += -D_WINDOWS

# override default, which is ASFLAGS = CFLAGS
AS	= ml.exe
ASFLAGS = -Cp -Sn -Zi -coff $(INCLUDES)

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


#
# Always set CPU_TAG on Linux, WINCE.
#
CPU_TAG = _$(CPU_ARCH)

