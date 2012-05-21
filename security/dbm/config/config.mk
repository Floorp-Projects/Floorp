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
#
# OSF1 V4.0D doesn't have snprintf but V5.0A does.
#
ifeq ($(OS_TARGET)$(OS_RELEASE),OSF1V4.0D)
HAVE_SNPRINTF =
endif
ifdef HAVE_SNPRINTF
DEFINES += -DHAVE_SNPRINTF
endif

ifeq (,$(filter-out IRIX Linux,$(OS_TARGET)))
DEFINES += -DHAVE_SYS_CDEFS_H
endif

ifeq (,$(filter-out DGUX NCR ReliantUNIX SCO_SV SCOOS UNIXWARE,$(OS_TARGET)))
DEFINES += -DHAVE_SYS_BYTEORDER_H
endif

#
# None of the platforms that we are interested in need to
# define HAVE_MEMORY_H.
#
