#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# Master <component>-specific source import/export directories        #
#######################################################################

#
# <user_source_tree> master import/export directory prefix
#

ifndef SOURCE_PREFIX
    ifndef BUILD_TREE
	SOURCE_PREFIX = $(CORE_DEPTH)/../dist
    else
	SOURCE_PREFIX = $(BUILD_TREE)/dist
    endif
endif

#
# <user_source_tree> cross-platform (xp) master import/export directory
#

ifndef SOURCE_XP_DIR
    SOURCE_XP_DIR        = $(SOURCE_PREFIX)
endif

#
# <user_source_tree> cross-platform (xp) import/export directories
#

SOURCE_CLASSES_DIR     = $(SOURCE_XP_DIR)/classes
SOURCE_CLASSES_DBG_DIR = $(SOURCE_XP_DIR)/classes_DBG
SOURCE_XPHEADERS_DIR   = $(SOURCE_XP_DIR)/public/$(MODULE)
SOURCE_XPPRIVATE_DIR   = $(SOURCE_XP_DIR)/private/$(MODULE)

ifdef BUILD_OPT
    IMPORT_XPCLASS_DIR = $(SOURCE_CLASSES_DIR)
else
    IMPORT_XPCLASS_DIR = $(SOURCE_CLASSES_DBG_DIR)
endif

#
# <user_source_tree> machine-dependent (md) master import/export directory
#

ifndef SOURCE_MD_DIR
    SOURCE_MD_DIR        = $(SOURCE_PREFIX)/$(PLATFORM)
endif

#
# <user_source_tree> machine-dependent (md) import/export directories
#

#This is where we install built executables and (for Windows only) DLLs.
ifndef SOURCE_BIN_DIR
    SOURCE_BIN_DIR       = $(SOURCE_MD_DIR)/bin
endif

#This is where we install built libraries (.a, .so, .lib).
ifndef SOURCE_LIB_DIR
    SOURCE_LIB_DIR       = $(SOURCE_MD_DIR)/lib
endif

# This is where NSPR header files are found.
ifndef SOURCE_MDHEADERS_DIR
    SOURCE_MDHEADERS_DIR = $(SOURCE_MD_DIR)/include
endif

#######################################################################
# Master <component>-specific source release directories and files    #
#######################################################################

#
# <user_source_tree> source-side master release directory prefix
# NOTE:  export control policy enforced for XP and MD files released to
#        the staging area
#

ifeq ($(POLICY), domestic)
    SOURCE_RELEASE_PREFIX = $(SOURCE_PREFIX)/release/domestic
else
    ifeq ($(POLICY), export)
	SOURCE_RELEASE_PREFIX = $(SOURCE_PREFIX)/release/export
    else
	ifeq ($(POLICY), france)
	    SOURCE_RELEASE_PREFIX = $(SOURCE_PREFIX)/release/france
	else
#We shouldn't have to put another directory under here, but without it the perl
#script for releasing doesn't find the directory. It thinks it doesn't exist.
#So we're adding this no-policy directory so that the script for releasing works
#in all casese when policy is not set. This doesn't affect where the final jar
#files land, only where they are placed in the local tree when building the jar
#files. When there is no policy, the jar files will still land in
#<dist>/<module>/<date>/<platform> like they used to.
	    SOURCE_RELEASE_PREFIX = $(SOURCE_PREFIX)/release/no-policy
	endif
    endif
endif

#
# <user_source_tree> cross-platform (xp) source-side master release directory
#

SOURCE_RELEASE_XP_DIR = $(SOURCE_RELEASE_PREFIX)

#
# <user_source_tree> cross-platform (xp) source-side release directories
#

SOURCE_RELEASE_CLASSES_DIR     = classes
SOURCE_RELEASE_CLASSES_DBG_DIR = classes_DBG
SOURCE_RELEASE_XPHEADERS_DIR   = include

#
# <user_source_tree> cross-platform (xp) JAR source-side release files
#

XPCLASS_JAR     = xpclass.jar
XPCLASS_DBG_JAR = xpclass_dbg.jar
XPHEADER_JAR    = xpheader.jar

ifdef BUILD_OPT
    SOURCE_RELEASE_XP_CLASSES_DIR = $(SOURCE_RELEASE_CLASSES_DIR)
    IMPORT_XPCLASS_JAR = $(XPCLASS_JAR)
else
    SOURCE_RELEASE_XP_CLASSES_DIR = $(SOURCE_RELEASE_CLASSES_DBG_DIR)
    IMPORT_XPCLASS_JAR = $(XPCLASS_DBG_JAR)
endif

#
# <user_source_tree> machine-dependent (md) source-side master release directory
#

SOURCE_RELEASE_MD_DIR = $(PLATFORM)

#
# <user_source_tree> machine-dependent (md) source-side release directories
#

SOURCE_RELEASE_BIN_DIR       = $(PLATFORM)/bin
SOURCE_RELEASE_LIB_DIR       = $(PLATFORM)/lib
SOURCE_RELEASE_MDHEADERS_DIR = $(PLATFORM)/include
SOURCE_RELEASE_SPEC_DIR      = $(SOURCE_RELEASE_MD_DIR)

#
# <user_source_tree> machine-dependent (md) JAR/tar source-side release files
#

MDBINARY_JAR = mdbinary.jar
MDHEADER_JAR = mdheader.jar


# Where to put the results

ifneq ($(RESULTS_DIR),)
    RESULTS_DIR = $(RELEASE_TREE)/sectools/results
endif

MK_SOURCE = included
