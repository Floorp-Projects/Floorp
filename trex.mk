#!gmake
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

######################################################################
#
# This is a unix based makefile (currently) designed to pull
# and build all appropriate components and systems to generate
# a Trex distribution and products using such a distribution
#
######################################################################

IGNORE_MANIFEST=1
DEPTH=.

#
# Command macro defines
#

CWD=`pwd`
TOP=$(CWD)/..

CVS=cvs -q co -P
CVTREX = $(CVSROOT:pub=src)
CVST=cvs -q -d $(CVTREX) co -P

default:  pull_all build_all

pull_all: pull_platform pull_trex 

pull_platform:
	cd $(MOZ_SRC); \
	$(CVS) mozilla/config mozilla/nglayout.mk; \
	cd mozilla ;\
	gmake -f nglayout.mk pull_all;\
	cd $(MOZ_SRC)

pull_trex:
	cd $(MOZ_SRC); \
	$(CVS)  mozilla/gconfig; \
	$(CVS)  mozilla/shell; \
	cd $(MOZ_SRC)/mozilla; \
	$(CVST)  -d trex ns/trex; \
	cd $(MOZ_SRC)/.

build_all: build_platform build_trex

build_platform:
	cd $(MOZ_SRC)/mozilla; \
	gmake -f nglayout.mk real_all; \
	cd $(MOZ_SRC)


build_trex:
	cd $(MOZ_SRC)/mozilla/shell; \
	gmake; \
	cd $(MOZ_SRC)/mozilla/trex; \
	gmake; \
	cd $(MOZ_SRC)













