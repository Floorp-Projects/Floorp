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

#######################################################################
# Master "Core Components" file system "release" prefixes             #
#######################################################################

#	RELEASE_TREE = $(CORE_DEPTH)/../coredist


ifndef RELEASE_TREE
	ifdef BUILD_SHIP
		ifdef USE_SHIPS 
			RELEASE_TREE = $(BUILD_SHIP)
		else
			RELEASE_TREE = /m/dist
		endif
	else
		RELEASE_TREE = /m/dist
	endif
		ifeq ($(OS_TARGET), WINNT)
		ifdef BUILD_SHIP
			ifdef USE_SHIPS
				RELEASE_TREE = $(NTBUILD_SHIP)
			else
				RELEASE_TREE = //iridium/components
			endif
		else
			RELEASE_TREE = //iridium/components
		endif
		endif
	
	ifeq ($(OS_TARGET), WIN95)
		ifdef BUILD_SHIP
			ifdef USE_SHIPS
				RELEASE_TREE = $(NTBUILD_SHIP)
			else
				RELEASE_TREE = //iridium/components
			endif
		else
			RELEASE_TREE = //iridium/components
		endif
	endif
	ifeq ($(OS_TARGET), WIN16)
	ifdef BUILD_SHIP
		ifdef USE_SHIPS
			RELEASE_TREE = $(NTBUILD_SHIP)
		else
			RELEASE_TREE = //iridium/components
		endif
	else
		RELEASE_TREE = //iridium/components
	endif
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
