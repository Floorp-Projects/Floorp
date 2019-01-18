#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_WIDGET_TOOLKIT = os2

# XP_PC is for Window and OS2 on Intel X86
# XP_OS2 is strictly for OS2 only
XP_DEFINE  += -DXP_PC=1  -DXP_OS2=1

# Override prefix
LIB_PREFIX  = $(NULL)

# Override suffix in suffix.mk
LIB_SUFFIX  = lib
# the DLL_SUFFIX must be uppercase for FIPS mode to work. bugzilla 240784
DLL_SUFFIX  = DLL
PROG_SUFFIX = .exe


CCC			= gcc
LD  			= gcc
AR                      = emxomfar r $@
# Keep AR_FLAGS blank so that we do not have to change rules.mk
AR_FLAGS                = 
RANLIB 			= @echo OS2 RANLIB
BSDECHO 		= @echo OS2 BSDECHO
IMPLIB			= emximp -o
FILTER			= emxexp -o

# GCC for OS/2 currently predefines these, but we don't want them
DEFINES 		+= -Uunix -U__unix -U__unix__

DEFINES			+= -DTCPV40HDRS

ifeq ($(MOZ_OS2_HIGH_MEMORY),1)
HIGHMEM_LDFLAG          = -Zhigh-mem
endif

ifndef NO_SHARED_LIB
DSO_CFLAGS              = 
DSO_PIC_CFLAGS          = 
MKSHLIB                 = $(CXX) $(CXXFLAGS) $(DSO_LDOPTS) -o $@
MKCSHLIB                = $(CC) $(CFLAGS) $(DSO_LDOPTS) -o $@
MKSHLIB_FORCE_ALL       = 
MKSHLIB_UNFORCE_ALL     = 
DSO_LDOPTS              = -Zomf -Zdll -Zmap $(HIGHMEM_LDFLAG)
SHLIB_LDSTARTFILE	= 
SHLIB_LDENDFILE		= 
ifdef MAPFILE
MKSHLIB += $(MAPFILE)
endif
PROCESS_MAP_FILE = \
	echo LIBRARY $(LIBRARY_NAME)$(LIBRARY_VERSION) INITINSTANCE TERMINSTANCE > $@; \
	echo PROTMODE >> $@; \
	echo CODE    LOADONCALL MOVEABLE DISCARDABLE >> $@; \
	echo DATA    PRELOAD MOVEABLE MULTIPLE NONSHARED >> $@; \
	echo EXPORTS >> $@; \
	grep -v ';+' $< | grep -v ';-' | \
	sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,\([\t ]*\),\1_,' | \
	awk 'BEGIN {ord=1;} { print($$0 " @" ord " RESIDENTNAME"); ord++;}' >> $@

endif   #NO_SHARED_LIB

OS_CFLAGS          = -Wall -Wno-unused -Wpointer-arith -Wcast-align -Wno-switch -Zomf -DDEBUG -DTRACING -g

ifdef BUILD_OPT
ifeq (11,$(ALLOW_OPT_CODE_SIZE)$(OPT_CODE_SIZE))
	OPTIMIZER += -Os -s
else
	OPTIMIZER += -O2 -s
endif
DEFINES 		+= -UDEBUG -U_DEBUG -DNDEBUG
DLLFLAGS		= -DLL -OUT:$@ -MAP:$(@:.dll=.map) $(HIGHMEM_LDFLAG)
EXEFLAGS    		= -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE $(HIGHMEM_LDFLAG)
OBJDIR_TAG 		= _OPT
else
#OPTIMIZER		= -O+ -Oi
DEFINES 		+= -DDEBUG -D_DEBUG -DDEBUGPRINTS     #HCT Need += to avoid overidding manifest.mn 
DLLFLAGS		= -DEBUG -DLL -OUT:$@ -MAP:$(@:.dll=.map) $(HIGHMEM_LDFLAG)
EXEFLAGS    		= -DEBUG -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE $(HIGHMEM_LDFLAG)
OBJDIR_TAG 		= _DBG
LDFLAGS 		= -DEBUG $(HIGHMEM_LDFLAG)
endif   # BUILD_OPT

# OS/2 use nsinstall that is included in the toolkit.
# since we do not wish to support and maintain 3 version of nsinstall in mozilla, nspr and nss

ifdef BUILD_TREE
NSINSTALL_DIR  = $(BUILD_TREE)/nss
else
NSINSTALL_DIR  = $(CORE_DEPTH)/coreconf/nsinstall
endif
# NSINSTALL      = $(NSINSTALL_DIR)/$(OBJDIR_NAME)/nsinstall
NSINSTALL 	= nsinstall             # HCT4OS2
INSTALL		= $(NSINSTALL)

MKDEPEND_DIR    = $(CORE_DEPTH)/coreconf/mkdepend
MKDEPEND        = $(MKDEPEND_DIR)/$(OBJDIR_NAME)/mkdepend
MKDEPENDENCIES  = $(OBJDIR_NAME)/depend.mk

####################################################################
#
# One can define the makefile variable NSDISTMODE to control
# how files are published to the 'dist' directory.  If not
# defined, the default is "install using relative symbolic
# links".  The two possible values are "copy", which copies files
# but preserves source mtime, and "absolute_symlink", which
# installs using absolute symbolic links.
#   - THIS IS NOT PART OF THE NEW BINARY RELEASE PLAN for 9/30/97
#   - WE'RE KEEPING IT ONLY FOR BACKWARDS COMPATIBILITY
####################################################################

ifeq ($(NSDISTMODE),copy)
	# copy files, but preserve source mtime
	INSTALL  = $(NSINSTALL)
	INSTALL += -t
else
	ifeq ($(NSDISTMODE),absolute_symlink)
		# install using absolute symbolic links
		INSTALL  = $(NSINSTALL)
		INSTALL += -L `pwd`
	else
		# install using relative symbolic links
		INSTALL  = $(NSINSTALL)
		INSTALL += -R
	endif
endif

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
endef

#
# override the definition of DLL_PREFIX in prefix.mk
#

ifndef DLL_PREFIX
    DLL_PREFIX = $(NULL)
endif

#
# override the TARGETS defined in ruleset.mk, adding IMPORT_LIBRARY
#
ifndef TARGETS
    TARGETS = $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(PROGRAM)
endif


ifdef LIBRARY_NAME
    IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)$(JDK_DEBUG_SUFFIX).lib
endif

