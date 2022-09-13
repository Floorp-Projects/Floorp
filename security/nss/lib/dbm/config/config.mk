#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# These macros are defined by mozilla's configure script.
# We define them manually here.
#

DEFINES += -DSTDC_HEADERS -DHAVE_STRERROR

#
# Most platforms have snprintf, so it's simpler to list the exceptions.
#
HAVE_SNPRINTF = 1
DEFINES += -DHAVE_SNPRINTF

ifeq (,$(filter-out NCR ReliantUNIX SCO_SV SCOOS UNIXWARE,$(OS_TARGET)))
DEFINES += -DHAVE_SYS_BYTEORDER_H
endif

#
# None of the platforms that we are interested in need to
# define HAVE_MEMORY_H.
#
