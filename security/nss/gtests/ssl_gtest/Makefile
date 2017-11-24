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

include ../common/gtest.mk

CFLAGS += -I$(CORE_DEPTH)/lib/ssl

ifdef NSS_DISABLE_TLS_1_3
NSS_DISABLE_TLS_1_3=1
# Run parameterized tests only, for which we can easily exclude TLS 1.3
CPPSRCS := $(filter-out $(shell grep -l '^TEST_F' $(CPPSRCS)), $(CPPSRCS))
CFLAGS += -DNSS_DISABLE_TLS_1_3
endif

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
