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

MOZ_WIDGET_TOOLKIT = os2

# Specify toolset.  Default to EMX.
ifeq ($(MOZ_OS2_TOOLS),VACPP)
XP_OS2_VACPP = 1
else
ifeq ($(MOZ_OS2_TOOLS),PGCC)
XP_OS2_EMX   = 1
else
MOZ_OS2_TOOLS = EMX
XP_OS2_EMX   = 1
endif
endif

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


ifdef XP_OS2_EMX

CCC			= gcc
LINK			= gcc
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

ifndef NO_SHARED_LIB
WRAP_MALLOC_LIB         = 
WRAP_MALLOC_CFLAGS      = 
DSO_CFLAGS              = 
DSO_PIC_CFLAGS          = 
MKSHLIB                 = $(CXX) $(CXXFLAGS) $(DSO_LDOPTS) -o $@
MKCSHLIB                = $(CC) $(CFLAGS) $(DSO_LDOPTS) -o $@
MKSHLIB_FORCE_ALL       = 
MKSHLIB_UNFORCE_ALL     = 
DSO_LDOPTS              = -Zomf -Zdll -Zmap
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

OS_CFLAGS          = -Wall -W -Wno-unused -Wpointer-arith -Wcast-align -Zomf -DDEBUG -DTRACING -g

ifdef BUILD_OPT
OPTIMIZER		= -O2 -s
DEFINES 		+= -UDEBUG -U_DEBUG -DNDEBUG
DLLFLAGS		= -DLL -OUT:$@ -MAP:$(@:.dll=.map)
EXEFLAGS    		= -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE
OBJDIR_TAG 		= _OPT
else
#OPTIMIZER		= -O+ -Oi
DEFINES 		+= -DDEBUG -D_DEBUG -DDEBUGPRINTS     #HCT Need += to avoid overidding manifest.mn 
DLLFLAGS		= -DEBUG -DLL -OUT:$@ -MAP:$(@:.dll=.map)
EXEFLAGS    		= -DEBUG -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE
OBJDIR_TAG 		= _DBG
LDFLAGS 		= -DEBUG 
endif   # BUILD_OPT

else    # XP_OS2_VACPP

# Override suffix in suffix.mk
OBJ_SUFFIX  = .obj
ASM_SUFFIX  = .asm

AS = alp.exe
ifdef BUILD_OPT
ASFLAGS = -Od
else
ASFLAGS = +Od
endif
CCC			= icc -q -DXP_OS2 -DOS2=4 -N10
LINK			= -ilink
AR		= -ilib /NOL /NOI /O:$(subst /,\\,$@)
# Keep AR_FLAGS blank so that we do not have to change rules.mk
AR_FLAGS                = 
RANLIB 			= @echo OS2 RANLIB
BSDECHO 		= @echo OS2 BSDECHO
IMPLIB    = implib /NOL /NOI
FILTER    = cppfilt -b -p -q

ifndef NO_SHARED_LIB
WRAP_MALLOC_LIB         = 
WRAP_MALLOC_CFLAGS      = 
DSO_CFLAGS              = 
DSO_PIC_CFLAGS          = 
MKSHLIB                 = $(LD) $(DSO_LDOPTS)
MKCSHLIB                = $(LD) $(DSO_LDOPTS)
MKSHLIB_FORCE_ALL       = 
MKSHLIB_UNFORCE_ALL     = 
DSO_LDOPTS              = 
# DLL_SUFFIX              = .dll
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
	sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' >> $@
endif   #NO_SHARED_LIB

OS_CFLAGS          = /Q /qlibansi /Gd /Gm /Su4 /Mp /Tl-
INCLUDES        += -I$(CORE_DEPTH)/../dist/include
DEFINES         += -DXP_OS2_VACPP -DTCPV40HDRS

DLLFLAGS    = /DLL /O:$@ /INC:_dllentry /MAP:$(@:.dll=.map)
EXEFLAGS    = -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE
LDFLAGS     = /FREE /NOE /LINENUMBERS /nologo

ifdef BUILD_OPT
OPTIMIZER		= /O+ /Gl+ /G5 /qarch=pentium
DEFINES 		+= -UDEBUG -U_DEBUG -DNDEBUG
OBJDIR_TAG 		= _OPT
LDFLAGS     += /NODEBUG /OPTFUNC /EXEPACK:2 /PACKCODE /PACKDATA
else
OS_CFLAGS   += /Ti+
DEFINES 		+= -DDEBUG -D_DEBUG -DDEBUGPRINTS     #HCT Need += to avoid overidding manifest.mn 
DLLFLAGS    += /DE
EXEFLAGS    += /DE
OBJDIR_TAG 		= _DBG
LDFLAGS     += /DE
endif   # BUILD_OPT

endif   # XP_OS2_VACPP

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
# installs using absolute symbolic links.  The "absolute_symlink"
# option requires NFSPWD.
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
		INSTALL += -L `$(NFSPWD)`
	else
		# install using relative symbolic links
		INSTALL  = $(NSINSTALL)
		INSTALL += -R
	endif
endif

DEFINES += -DXP_OS2

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

