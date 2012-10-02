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
include config.mk

ifeq ($(OS_TARGET)$(OS_RELEASE), AIX4.2)
OS_LIBS += -lsvld 
endif 

ifeq ($(OS_TARGET)$(OS_RELEASE), SunOS5.6)
OS_LIBS += -ldl -lxnet -lposix4 -lsocket -lnsl
endif

EXTRA_LIBS += $(DIST)/lib/$(LIB_PREFIX)crmf.$(LIB_SUFFIX)

include ../platlibs.mk

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

LDDIST = $(DIST)/lib

ifeq (,$(filter-out WIN%,$(OS_TARGET)))
EXTRA_LIBS += $(LDDIST)/sectool.lib
endif

include ../platrules.mk
