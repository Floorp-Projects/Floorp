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
NETLIB_BRANCH = MODULAR_NETLIB_BRANCH
XPCOM_BRANCH = XPCOM_BRANCH
RAPTOR_BRANCH = RAPTOR_BRANCH

# CVS commands to pull the appropriate branch versions
CVSCO_XPCOM = $(CVSCO) -r $(XPCOM_BRANCH)
CVSCO_IMGLIB = $(CVSCO) -r $(IMGLIB_BRANCH)
CVSCO_NETLIB = $(CVSCO) -r $(NETLIB_BRANCH)
CVSCO_RAPTOR = $(CVSCO)
CVSCO_LIZARD = $(CVSCO)

# The list of directories that need to be built to build the standalone
# raptor test program. The order is important.
ifndef RAPTOR_PASS2
DIRS =				\
  config			\
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
  lib/libnet			\
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

THIS_MAKEFILE = raptor.mk

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
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 export; \
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 libs; \
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 install

real_export:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) export; \
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 export

real_libs:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) libs; \
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 libs

real_install:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) install; \
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 install

real_clobber:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) clobber_all; \
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 clobber_all; \
	$(MAKE) -f $(THIS_MAKEFILE) final_clobber

final_clobber:
	cd $(MOZ_SRC)/mozilla; \
	$(RM) -r dist

real_depend:
	cd $(MOZ_SRC)/mozilla; \
	$(MAKE) -f $(THIS_MAKEFILE) depend; \
	$(MAKE) -f $(THIS_MAKEFILE) RAPTOR_PASS2=pass2 depend

include $(DEPTH)/config/rules.mk

#
# Rules for pulling the source from the cvs repository
#

pull_all: pull_lizard pull_xpcom pull_imglib pull_netlib pull_raptor 

pull_lizard:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_LIZARD) mozilla/config; \
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
	$(CVSCO_XPCOM) mozilla/modules/libreg; \
	$(CVSCO_XPCOM) mozilla/xpcom; \
	$(CVSCO_XPCOM) mozilla/modules/libpref

pull_imglib:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_IMGLIB) mozilla/jpeg; \
	$(CVSCO_IMGLIB) mozilla/modules/libutil; \
	$(CVSCO_IMGLIB) mozilla/modules/libimg 

pull_netlib:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_NETLIB) mozilla/lib/xp; \
	$(CVSCO_NETLIB) mozilla/lib/libnet; \
	$(CVSCO_NETLIB) mozilla/include

pull_raptor:
	cd $(MOZ_SRC)/.; \
	$(CVSCO_RAPTOR) mozilla/base; \
	$(CVSCO_RAPTOR) mozilla/dom; \
	$(CVSCO_RAPTOR) mozilla/gfx; \
	$(CVSCO_RAPTOR) mozilla/htmlparser; \
	$(CVSCO_RAPTOR) mozilla/layout; \
	$(CVSCO_RAPTOR) mozilla/view; \
	$(CVSCO_RAPTOR) mozilla/webshell; \
	$(CVSCO_RAPTOR) mozilla/widget
