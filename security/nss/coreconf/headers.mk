#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# Master "Core Components" include switch for support header files    #
#######################################################################

#
#  Always append source-side machine-dependent (md) and cross-platform
#  (xp) include paths
#

INCLUDES += -I$(SOURCE_MDHEADERS_DIR) -I$(SOURCE_XPHEADERS_DIR)

#
#  Only append source-side private cross-platform include paths for
#  sectools
#

INCLUDES += -I$(SOURCE_XPPRIVATE_DIR)

MK_HEADERS = included
