#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

XP_DEFINE  += -DXP_UNIX
LIB_SUFFIX  = a
DLL_SUFFIX  = so
AR          = ar cr $@
LDOPTS     += -L$(SOURCE_LIB_DIR)

ifdef BUILD_OPT
	OPTIMIZER  += -O
	DEFINES    += -UDEBUG -DNDEBUG
else
	OPTIMIZER  += -g
	USERNAME   := $(shell whoami)
	USERNAME   := $(subst -,_,$(USERNAME))
	DEFINES    += -DDEBUG -UNDEBUG -DDEBUG_$(USERNAME)
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
