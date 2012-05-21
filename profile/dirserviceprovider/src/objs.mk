#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MODULES_PROFILEDIRSERVICE_SRC_LCSRCS = \
		nsProfileDirServiceProvider.cpp \
		$(NULL)

ifdef MOZ_PROFILELOCKING
MODULES_PROFILEDIRSERVICE_SRC_LCSRCS += nsProfileLock.cpp
endif

MODULES_PROFILEDIRSERVICE_SRC_CSRCS := $(addprefix $(topsrcdir)/profile/dirserviceprovider/src/, $(MODULES_PROFILEDIRSERVICE_SRC_LCSRCS))
