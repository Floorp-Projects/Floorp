#
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
#

DEPTH		= .
THIS_MAKEFILE	= nglayout.mk

# Branch tags we use
LIBPREF_BRANCH	= XPCOM_BRANCH
NETLIB_BRANCH	=
PLUGIN_BRANCH	=

ifdef MOZ_DATE
CVSCO		= cvs -q co -P -D $(MOZ_DATE)
CVSCO_PLUGIN	= $(CVSCO)
else
CVSCO		= cvs -q co -P
CVSCO_PLUGIN	= $(CVSCO) -A
endif

CVSCO_IMGLIB	= $(CVSCO)
CVSCO_LIBPREF	= $(CVSCO) -r $(LIBPREF_BRANCH)
CVSCO_LIZARD	= $(CVSCO)
CVSCO_NETLIB	= $(CVSCO)
CVSCO_NGLAYOUT	= $(CVSCO)

# The list of directories that need to be built to build the standalone
# nglayout test program. The order is important.
DIRS		= \
		config \
		dbm \
		nsprpub \
		jpeg \
		modules/libreg \
		xpcom \
		modules/zlib \
		modules/libutil \
		sun-java \
		nav-java \
		js \
		modules/security/freenav \
		modules/libpref \
		modules/libimg \
		modules/oji \
		modules/plugin \
		base \
		caps \
		lib/xp \
		lib/libpwcac \
		network \
		lib/liblayer/include \
		htmlparser \
		gfx \
		dom \
		view \
		widget \
		layout \
		editor \
		webshell

real_all: all

real_export: export

real_libs: libs

real_install: install

real_clobber: clobber

real_depend: depend

#
# Rules for pulling the source from the cvs repository
#
pull_all: pull_lizard pull_xpcom pull_imglib pull_netlib pull_nglayout pull_editor pull_plugin pull_autoconf

pull_lizard:
	cd $(DEPTH)/..; \
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
	$(CVSCO_LIZARD) mozilla/caps; \
	$(CVSCO_LIZARD) mozilla/modules/security/freenav; \
	$(CVSCO_LIZARD) mozilla/rdf;

pull_xpcom:
	cd $(DEPTH)/..; \
	$(CVSCO) -A mozilla/modules/libreg; \
	$(CVSCO) -A mozilla/xpcom; \
	$(CVSCO_LIBPREF) mozilla/modules/libpref

pull_imglib:
	cd $(DEPTH)/..; \
	$(CVSCO_IMGLIB) mozilla/jpeg; \
	$(CVSCO_IMGLIB) mozilla/modules/libutil; \
	$(CVSCO_IMGLIB) mozilla/modules/libimg 

pull_plugin:
	cd $(DEPTH)/..; \
	$(CVSCO_LIZARD) mozilla/modules/oji; \
	$(CVSCO_PLUGIN) mozilla/modules/plugin

pull_netlib:
	cd $(DEPTH)/..; \
	$(CVSCO_NETLIB) mozilla/lib/xp; \
	$(CVSCO_NETLIB) mozilla/lib/libpwcac; \
	$(CVSCO_NETLIB) mozilla/network; \
	$(CVSCO_NETLIB) mozilla/include

pull_nglayout:
	cd $(DEPTH)/..; \
	$(CVSCO_NGLAYOUT) mozilla/base; \
	$(CVSCO_NGLAYOUT) mozilla/dom; \
	$(CVSCO_NGLAYOUT) mozilla/gfx; \
	$(CVSCO_NGLAYOUT) mozilla/htmlparser; \
	$(CVSCO_NGLAYOUT) mozilla/layout; \
	$(CVSCO_NGLAYOUT) mozilla/view; \
	$(CVSCO_NGLAYOUT) mozilla/webshell; \
	$(CVSCO_NGLAYOUT) mozilla/widget

pull_editor:
	cd $(DEPTH)/..; \
	$(CVSCO_NGLAYOUT) mozilla/editor

pull_doc:
	cd $(DEPTH)/..; \
	$(CVSCO_NGLAYOUT) README/nglayout; \
	$(CVSCO_NGLAYOUT) mozilla/LICENSE; \
	$(CVSCO_NGLAYOUT) mozilla/LEGAL

pull_autoconf:
	cd $(DEPTH)/..; \
	$(CVSCO_NGLAYOUT) mozilla/build/autoconf; \
	$(CVSCO_NGLAYOUT) mozilla/build/hcc; \
	$(CVSCO_NGLAYOUT) mozilla/build/hcpp; \
	$(CVSCO_NGLAYOUT) mozilla/xpfe; \
	$(CVSCO_NGLAYOUT) mozilla/Makefile.in; \
	$(CVSCO_NGLAYOUT) mozilla/aclocal.m4; \
	$(CVSCO_NGLAYOUT) mozilla/configure.in;

######################################################################
#
# Build tarball

DATE_CMD	= date
DATE		= $(shell $(DATE_CMD) +%Y%m%d)

TAR		= tar
GZIP		= gzip

TARBALL		= $(MOZ_SRC)/unix-$(DATE).tar

TARFILES	= mozilla README/nglayout

tarball: pull_all pull_doc clobber clobber_all real_tar

real_tar:
	@echo Making $(TARBALL)
	cd $(DEPTH)/..; \
	rm -f $(TARBALL) $(TARBALL).gz; \
	$(TAR) cf $(TARBALL) $(TARFILES)
	@echo Making gzip of $(TARBALL); \
	cd $(DEPTH)/..; \
	$(GZIP) -9 -q $(TARBALL)

