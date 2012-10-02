#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
#  Override TARGETS variable so that only static libraries
#  are specifed as dependencies within rules.mk.
#
#  DEFINES+=-DPKIX_LISTDEBUG Can be used to turn on debug compilation

TARGETS        = $(LIBRARY)
SHARED_LIBRARY =
IMPORT_LIBRARY =
PROGRAM        =

