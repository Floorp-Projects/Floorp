#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile-tlsu.com,v 1.2 2004/11/29 16:31:47 christophe.ravel.bugs%sun.com Exp $"
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

PRODUCT_VERSION = $(shell grep NSS_VERSION $(CORE_DEPTH)/nss/lib/nss/nss.h    | sed -e 's/"$$//' -e 's/.*"//' -e 's/ .*//')

LN = /usr/bin/ln

CLOBBERFILES = $(FILES)

include $(CORE_DEPTH)/coreconf/config.mk
include $(CORE_DEPTH)/coreconf/rules.mk

# vim: ft=make
