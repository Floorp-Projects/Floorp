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

#######################################################################
# Build master "Core Components" release version directory name       #
#######################################################################

#
# Always set CURRENT_VERSION_SYMLINK to the <current> symbolic link.
#

CURRENT_VERSION_SYMLINK = current


#
#  For the sake of backwards compatibility (*sigh*) ...
#

ifndef VERSION
    ifdef BUILD_NUM
	VERSION = $(BUILD_NUM)
    endif
endif

ifndef RELEASE_VERSION
    ifdef BUILD_NUM
	RELEASE_VERSION = $(BUILD_NUM)
    endif
endif

#
# If VERSION has still NOT been set on the command line,
# as an environment variable, by the individual Makefile, or
# by the <component>-specific "version.mk" file, set VERSION equal
# to $(CURRENT_VERSION_SYMLINK).

ifndef VERSION
    VERSION = $(CURRENT_VERSION_SYMLINK)
endif

# If RELEASE_VERSION has still NOT been set on the command line,
# as an environment variable, by the individual Makefile, or
# by the <component>-specific "version.mk" file, automatically
# generate the next available version number via a perl script.
# 

ifndef RELEASE_VERSION
    RELEASE_VERSION = 
endif

#
# Set <component>-specific versions for compiliation and linkage.
#

ifndef JAVA_VERSION
    JAVA_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef NETLIB_VERSION
    NETLIB_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef NSPR_VERSION
    NSPR_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef SECTOOLS_VERSION
    SECTOOLS_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef SECURITY_VERSION
    SECURITY_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

MK_VERSION = included
