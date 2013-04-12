#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#ident	"$Id$"
#

MACH = $(shell mach)

PUBLISH_ROOT = $(DIST)
ifeq ($(CORE_DEPTH),../../..)
ROOT = ROOT
else
ROOT = $(subst ../../../,,$(CORE_DEPTH))/ROOT
endif

PKGARCHIVE = $(PUBLISH_ROOT)/pkgarchive
DATAFILES = copyright
FILES = $(DATAFILES) pkginfo


PACKAGE = $(shell basename `pwd`)

PRODUCT_VERSION = $(shell grep NSS_VERSION $(CORE_DEPTH)/nss/lib/nss/nss.h \
    | head -1 \
    | sed -e 's/"$$//' -e 's/.*"//' -e 's/ .*//')

LN = /usr/bin/ln

CLOBBERFILES = $(FILES)

include $(CORE_DEPTH)/coreconf/config.mk
include $(CORE_DEPTH)/coreconf/rules.mk

# vim: ft=make
