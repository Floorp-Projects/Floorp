#
# Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile.com,v 1.4 2003/02/08 22:43:37 sonja.mirtitsch%sun.com Exp $"
#

MACH = $(shell mach)

PUBLISH_ROOT = $(DIST)
ifeq ($(MOD_DEPTH),../..)
ROOT = ROOT
else
ROOT = $(subst ../../,,$(MOD_DEPTH))/ROOT
endif

PKGARCHIVE = $(dist_libdir)/pkgarchive
DATAFILES = copyright
FILES = $(DATAFILES) pkginfo prototype

PACKAGE = $(shell basename `pwd`)

PRODUCT_VERSION =  $(shell echo `grep PR_VERSION $(MOD_DEPTH)/dist/include/nspr/prinit.h \
                       | sed -e 's/"$$//' -e 's/.*"//' -e 's/ .*//'`)

LN = /usr/bin/ln

CLOBBERFILES = $(FILES)

include $(topsrcdir)/config/rules.mk

# vim: ft=make



