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

XP_DEFINE  += -DXP_UNIX
LIB_SUFFIX  = a
DLL_SUFFIX  = so
AR          = ar
AR         += cr $@
LDOPTS     += -L$(SOURCE_LIB_DIR)

ifdef BUILD_OPT
	OPTIMIZER  += -O
	DEFINES    += -UDEBUG -DNDEBUG
else
	OPTIMIZER  += -g
	DEFINES    += -DDEBUG -UNDEBUG -DDEBUG_$(shell whoami)
endif

ifdef BUILD_TREE
NSINSTALL_DIR  = $(BUILD_TREE)/nss
NSINSTALL      = $(BUILD_TREE)/nss/nsinstall
else
NSINSTALL_DIR  = $(CORE_DEPTH)/coreconf/nsinstall
NSINSTALL      = $(NSINSTALL_DIR)/$(OBJDIR_NAME)/nsinstall
endif

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

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
endef
