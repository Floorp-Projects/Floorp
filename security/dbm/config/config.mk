#! gmake
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

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
ifeq ($(OS_ARCH)$(OS_RELEASE),OSF1V4.0D)
HAVE_SNPRINTF =
endif
ifdef HAVE_SNPRINTF
DEFINES += -DHAVE_SNPRINTF
endif

ifeq (,$(filter-out IRIX Linux,$(OS_ARCH)))
DEFINES += -DHAVE_SYS_CDEFS_H
endif

ifeq (,$(filter-out DGUX NCR ReliantUNIX SCO_SV SCOOS UNIXWARE,$(OS_ARCH)))
DEFINES += -DHAVE_SYS_BYTEORDER_H
endif

#
# None of the platforms that we are interested in need to
# define HAVE_MEMORY_H.
#
