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

DEPTH=.

#
# Command macro defines
#

CVSCO = cvs -q co -P

# Branch tags we use
IMGLIB_BRANCH = MODULAR_IMGLIB_BRANCH
NETLIB_BRANCH = 
XPCOM_BRANCH = XPCOM_BRANCH

# CVS commands to pull the appropriate branch versions
CVSCO_XPCOM = $(CVSCO) -r $(XPCOM_BRANCH)
CVSCO_IMGLIB = $(CVSCO) -r $(IMGLIB_BRANCH)
CVSCO_NETLIB = $(CVSCO)
CVSCO_NGLAYOUT = $(CVSCO)
CVSCO_LIZARD = $(CVSCO)

# The list of directories that need to be built to build the standalone
# nglayout test program. The order is important.
ifndef NGLAYOUT_PASS2
DIRS =				\
  config			\
  dbm				\
  nsprpub			\
  jpeg				\
  modules/libreg		\
  xpcom				\
  modules/zlib			\
  modules/libutil		\
  sun-java			\
  nav-java			\
  js				\
  modules/security/freenav	\
  modules/libpref		\
  modules/libimg		\
  base				\
  lib/xp			\
  lib/libpwcac			\
  network			\
  lib/liblayer/include
else
DIRS =				\
  htmlparser			\
  dom				\
  gfx				\
  view				\
  widget			\
  layout			\
  webshell
endif

include $(DEPTH)/config/config.mk

#
# NOTE: Don't use make all with this makefile; it won't work!
# NOTE: Don't use make export with this makefile; it won't work!
# NOTE: Don't use make libs with this makefile; it won't work!
# NOTE: Don't use make clobber with this makefile; it won't work!
#

THIS_MAKEFILE = nglayout.mk

real_all: pass1_all pass2_all

pass1_all:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) export
	cd $(MOZ_SRC)/mozilla/base; \
	$(MAKE) export
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) libs; \
	$(MAKE) -f $(THIS_MAKEFILE) install

pass2_all: 
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 export; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 libs; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 install

real_export:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) export; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 export

real_libs:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) libs; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 libs

real_install:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) install; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 install

real_clobber:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) clobber_all; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 clobber_all; \
	$(MAKE) -f $(THIS_MAKEFILE) final_clobber

final_clobber:
	cd $(MOZ_SRC)/mozilla; \
	$(RM) -r dist

real_depend:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) depend; \
	$(MAKE) -f $(THIS_MAKEFILE) NGLAYOUT_PASS2=pass2 depend

include $(DEPTH)/config/rules.mk

#
# Rules for pulling the source from the cvs repository
#

pull_all: pull_lizard pull_xpcom pull_imglib pull_netlib pull_nglayout 

pull_lizard:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_LIZARD) mozilla/config; \
	$(CVSCO_LIZARD) mozilla/dbm; \
	$(CVSCO_LIZARD) mozilla/build/build_number; \
	$(CVSCO_LIZARD) mozilla/cmd/xfe/icons/icondata.h; \
	$(CVSCO_LIZARD) mozilla/lib/liblayer; \
	$(CVSCO_LIZARD) mozilla/modules/zlib; \
	$(CVSCO_LIZARD) mozilla/modules/libutil; \
	$(CVSCO_LIZARD) mozilla/nsprpub; \
	$(CVSCO_LIZARD) mozilla/sun-java; \
	$(CVSCO_LIZARD) mozilla/nav-java; \
	$(CVSCO_LIZARD) mozilla/js; \
	$(CVSCO_LIZARD) mozilla/modules/security/freenav; \

pull_xpcom:
	cd $(MOZ_SRC)/.; \
	$(CVSCO) -A mozilla/modules/libreg; \
	$(CVSCO) -A mozilla/xpcom; \
	$(CVSCO_XPCOM) mozilla/modules/libpref

pull_imglib:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_IMGLIB) mozilla/jpeg; \
	$(CVSCO_IMGLIB) mozilla/modules/libutil; \
	$(CVSCO_IMGLIB) mozilla/modules/libimg 

pull_netlib:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_NETLIB) mozilla/lib/xp; \
	$(CVSCO_NETLIB) mozilla/lib/libpwcac; \
	$(CVSCO_NETLIB) mozilla/network; \
	$(CVSCO_NETLIB) mozilla/include

pull_nglayout:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_NGLAYOUT) mozilla/base; \
	$(CVSCO_NGLAYOUT) mozilla/dom; \
	$(CVSCO_NGLAYOUT) mozilla/gfx; \
	$(CVSCO_NGLAYOUT) mozilla/htmlparser; \
	$(CVSCO_NGLAYOUT) mozilla/layout; \
	$(CVSCO_NGLAYOUT) mozilla/view; \
	$(CVSCO_NGLAYOUT) mozilla/webshell; \
	$(CVSCO_NGLAYOUT) mozilla/widget

pull_doc:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_NGLAYOUT) README/nglayout; \
	$(CVSCO_NGLAYOUT) mozilla/LICENSE; \
	$(CVSCO_NGLAYOUT) mozilla/LEGAL

######################################################################
#
# Build tarball

DATE_CMD=date
DATE=$(shell $(DATE_CMD) +%Y%m%d)

TAR = tar
GZIP = gzip

TARBALL = $(MOZ_SRC)/unix-$(DATE).tar

TARFILES = mozilla README/nglayout

tarball: pull_all pull_doc clobber clobber_all real_tar

real_tar:
	@echo Making $(TARBALL)
	cd $(MOZ_SRC)/.; \
	rm -f $(TARBALL) $(TARBALL).gz; \
	$(TAR) cf $(TARBALL) $(TARFILES)
	@echo Making gzip of $(TARBALL); \
	cd $(MOZ_SRC)/.; \
	$(GZIP) -9 -q $(TARBALL)
