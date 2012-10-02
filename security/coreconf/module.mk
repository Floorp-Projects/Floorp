#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# The master "Core Components" source and release component directory #
# names are ALWAYS identical and are the value of $(MODULE).          #
# NOTE:  A component is also called a module or a subsystem.          #
#######################################################################

#
#  All "Core Components" <component>-specific source-side tags must
#  always be identified for compiling/linking purposes
#

ifndef JAVA_SOURCE_COMPONENT
    JAVA_SOURCE_COMPONENT = java
endif

ifndef NETLIB_SOURCE_COMPONENT
    NETLIB_SOURCE_COMPONENT = netlib
endif

ifndef NSPR_SOURCE_COMPONENT
    NSPR_SOURCE_COMPONENT = nspr20
endif

ifndef SECTOOLS_SOURCE_COMPONENT
    SECTOOLS_SOURCE_COMPONENT = sectools
endif

ifndef SECURITY_SOURCE_COMPONENT
    SECURITY_SOURCE_COMPONENT = security
endif

MK_MODULE = included
