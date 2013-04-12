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

ifndef USE_SYSTEM_ZLIB
ZLIB_SRCDIR = zlib  # Add the zlib directory to DIRS.
endif

ifndef MOZILLA_CLIENT
ifndef NSS_USE_SYSTEM_SQLITE
SQLITE_SRCDIR = sqlite  # Add the sqlite directory to DIRS.
endif
endif

ifndef MOZILLA_CLIENT
ifeq ($(OS_ARCH),Linux)
SYSINIT_SRCDIR = sysinit  # Add the sysinit directory to DIRS.
endif
endif

ifndef NSS_DISABLE_DBM
DBM_SRCDIR = dbm  # Add the dbm directory to DIRS.
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

