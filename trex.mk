#!gmake
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

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
CVTREX = :pserver:$(USER)@cvsserver:/m/src
CVST=cvs -q -d $(CVTREX) co -P

LIBNLS_BRANCH           = -r libnls_v3_zulu_branch
LIBNLS_DIR              = ns/modules/libnls

TREX_MSGSDK_BRANCH      = 
TREX_MSGSDK_DIR         = mozilla/msgsdk


default:  pull_all build_all

pull_and_build_all: pull_all build_all

pull_all: pull_platform pull_julian pull_trex

pull_platform:
	cd $(MOZ_SRC); \
	$(CVS) mozilla/config mozilla/nglayout.mk; \
	cd mozilla ;\
	gmake -f nglayout.mk pull_all;\
	cd $(MOZ_SRC)

pull_julian:
ifneq ($(MOZ_ZULU_FREE),1)
	cd $(MOZ_SRC); \
	$(CVST) ns/client.mk; \
	cd $(MOZ_SRC)/ns/.; \
	$(CVST) -d config ns/clientconfig; \
	cd $(MOZ_SRC); \
	$(CVST) $(LIBNLS_BRANCH) $(LIBNLS_DIR); \
	cd $(MOZ_SRC)
else
	cd $(MOZ_SRC)
endif

pull_trex:
	cd $(MOZ_SRC); \
	$(CVS)  mozilla/gconfig; \
	$(CVS)  $(TREX_MSGSDK_BRANCH)   $(TREX_MSGSDK_DIR); \
	$(CVS)  mozilla/xpfc; \
	$(CVS)  mozilla/calendar; \
	cd $(MOZ_SRC)/.

build_all: build_platform build_julian build_trex

build_platform:
	cd $(MOZ_SRC)/mozilla; \
	gmake -f nglayout.mk real_all; \
	cd $(MOZ_SRC)


build_julian:: 
ifneq ($(MOZ_ZULU_FREE),1)
	cd $(MOZ_SRC)/ns/modules/libnls; \
	gmake; \
	cd $(MOZ_SRC)
else
	cd $(MOZ_SRC)
endif


build_trex:
	cd $(MOZ_SRC)/mozilla/msgsdk; \
	gmake buildC; \
	cd $(MOZ_SRC)/mozilla/gconfig; \
	gmake; \
	cd $(MOZ_SRC)/mozilla/xpfc; \
	gmake; \
	cd $(MOZ_SRC)/mozilla/calendar; \
	gmake; \
	cd $(MOZ_SRC)













