#
# Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile.com,v 1.2 2002/10/02 23:11:25 kirk.erickson%sun.com Exp $"
#

MACH = $(shell mach)

PUBLISH_ROOT = $(DIST)
ifeq ($(CORE_DEPTH),../../..)
ROOT = ROOT-$(OBJDIR_NAME)
else
ROOT = $(subst ../../../,,$(CORE_DEPTH))/ROOT-$(OBJDIR_NAME)
endif

PKGARCHIVE = $(PUBLISH_ROOT)/pkgarchive
DATAFILES = copyright
FILES = $(DATAFILES) pkginfo

PACKAGE = $(shell basename `pwd`)

PRODUCT_VERSION = 3.3.2
PRODUCT_NAME = NSS_3_3_2_RTM

LN = /usr/bin/ln

CLOBBERFILES = $(FILES)

include $(CORE_DEPTH)/coreconf/config.mk
include $(CORE_DEPTH)/coreconf/rules.mk

# vim: ft=make
