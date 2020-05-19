#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
