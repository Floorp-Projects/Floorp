#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# Master "Core Components" file system "release" prefixes             #
#######################################################################

# Windows platforms override this.  See WIN32.mk.
ifndef RELEASE_TREE
    ifdef BUILD_SHIP
	ifdef USE_SHIPS 
	    RELEASE_TREE = $(BUILD_SHIP)
	else
	    RELEASE_TREE = /share/builds/components
	endif
    else
	RELEASE_TREE = /share/builds/components
    endif
endif

#
# NOTE:  export control policy enforced for XP and MD files
#        released to the binary release tree
#

ifeq ($(POLICY), domestic)
    RELEASE_XP_DIR = domestic
    RELEASE_MD_DIR = domestic/$(PLATFORM)
else
    ifeq ($(POLICY), export)
	RELEASE_XP_DIR = export
	RELEASE_MD_DIR = export/$(PLATFORM)
    else
	ifeq ($(POLICY), france)
	    RELEASE_XP_DIR = france
	    RELEASE_MD_DIR = france/$(PLATFORM)
	else
	    RELEASE_XP_DIR = 
	    RELEASE_MD_DIR = $(PLATFORM)
	endif
    endif
endif


REPORTER_TREE = $(subst \,\\,$(RELEASE_TREE))

IMPORT_XP_DIR = 
IMPORT_MD_DIR = $(PLATFORM)

MK_TREE = included
