#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################



#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################


#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################


#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

MKSHLIB	= $(CCC) $(DSO_LDOPTS) $(DARWIN_SDK_SHLIBFLAGS)
ifeq (WINNT,$(OS_ARCH))
    # -EHsc because gtest has exception handlers
    OS_CFLAGS += -EHsc
    # On windows, we need to create the parent directory
    # Needed because we include files from a subdirectory
    MAKE_OBJDIR = $(INSTALL) -D $(dir $@)
endif
