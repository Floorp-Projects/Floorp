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

# Override suffix in suffix.mk
LIB_SUFFIX  = lib
DLL_SUFFIX  = dll
OBJ_SUFFIX  = .obj
ASM_SUFFIX  = .asm
PROG_SUFFIX = .exe


ifdef XP_OS2_EMX

#
# On OS/2 we proudly support gbash...
#
SHELL = GBASH.EXE
CCC			= gcc
LINK			= gcc
AR                      = emxomfar -p256 r $@
# Keep AR_FLAGS blank so that we do not have to change rules.mk
AR_FLAGS                = 
RANLIB 			= @echo OS2 RANLIB
BSDECHO 		= @echo OS2 BSDECHO
IMPLIB    = emximp -o
FILTER    = emxexp

ifndef NO_SHARED_LIB
WRAP_MALLOC_LIB         = 
WRAP_MALLOC_CFLAGS      = 
DSO_CFLAGS              = 
DSO_PIC_CFLAGS          = 
MKSHLIB                 = $(CXX) $(CXXFLAGS) $(DSO_LDOPTS) -o $@
MKCSHLIB                = $(CC) $(CFLAGS) $(DSO_LDOPTS) -o $@
MKSHLIB_FORCE_ALL       = 
MKSHLIB_UNFORCE_ALL     = 
DSO_LDOPTS              = -Zomf -Zdll -Zmt -Zcrtdll -Zlinker /NOO
# DLL_SUFFIX              = .dll
SHLIB_LDSTARTFILE	= 
SHLIB_LDENDFILE		= 
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = copy $(LIBRARY_NAME).def $@

endif   #NO_SHARED_LIB

OS_CFLAGS          = -Wall -W -Wno-unused -Wpointer-arith -Wcast-align -Zmtd -Zomf -Zmt  -DDEBUG -DDEBUG_wintrinh -DTRACING -g

# Where the libraries are
MOZ_COMPONENT_NSPR_LIBS=-L$(DIST)/lib $(NSPR_LIBS)
NSPR_LIBS	= -lplds4 -lplc4 -lnspr4 
NSPR_INCLUDE_DIR =   


ifdef BUILD_OPT
OPTIMIZER		= -O6 
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

#
# On OS/2 we proudly support gbash...
#
SHELL = GBASH.EXE
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
endif   #NO_SHARED_LIB

OS_CFLAGS          = /Q /qlibansi /Gd /Gm /Su4 /Mp /Tl-
INCLUDES        += -I$(CORE_DEPTH)/../dist/include
DEFINES         += -DXP_OS2_VACPP -DTCPV40HDRS

# Where the libraries are
NSPR_LIBS	= $(DIST)/lib/nspr4.lib $(DIST)/lib/plc4.lib $(DIST)/lib/plds4.lib
MOZ_COMPONENT_NSPR_LIBS=-L$(DIST)/lib $(NSPR_LIBS)
NSPR_INCLUDE_DIR =   


ifdef BUILD_OPT
OPTIMIZER		= -Oi -G5
DEFINES 		+= -UDEBUG -U_DEBUG -DNDEBUG
DLLFLAGS		= /DLL /O:$@ /INC:_dllentry /MAP:$(@:.dll=.map)
EXEFLAGS    		= -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE
OBJDIR_TAG 		= _OPT
LDFLAGS     = /FREE /NODEBUG /NOE /LINENUMBERS /nologo
else
OS_CFLAGS   += /Ti+
DEFINES 		+= -DDEBUG -D_DEBUG -DDEBUGPRINTS     #HCT Need += to avoid overidding manifest.mn 
DLLFLAGS		= /DEBUG /DLL /O:$@ /INC:_dllentry /MAP:$(@:.dll=.map)
EXEFLAGS    		= -DEBUG -PMTYPE:VIO -OUT:$@ -MAP:$(@:.exe=.map) -nologo -NOE
OBJDIR_TAG 		= _DBG
LDFLAGS 		= /FREE /DE /NOE /LINENUMBERS /nologo 
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
