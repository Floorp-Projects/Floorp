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
IGNORE_MANIFEST=1
THIS_MAKEFILE=raptor.mak
THAT_MAKEFILE=makefile.win

include <$(DEPTH)\config\config.mak>

# This section is copied from rules.mak

!if "$(WINOS)" == "WIN95"
W95MAKE=$(MOZ_SRC)\ns\config\w95make.exe
W32OBJS = $(OBJS:.obj=.obj, )
W32LOBJS = $(OBJS: .= +-.)
!endif

######################################################################

# This makefile is designed to make building the raptor viewer
# application easy.

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

# The list of directories that need to be built to build the
# standalone raptor test program. The order is important. The
# DIST_DIRS need to be built before the RAPTOR_DIRS.

DIST_DIRS =			\
  nsprpub			\
  include			\
  jpeg				\
  modules\libreg		\
  xpcom				\
  modules\zlib			\
  modules\libutil		\
  sun-java			\
  nav-java			\
  js				\
  modules\security\freenav	\
  modules\libpref		\
  modules\libimg		\
  lib\xp			\
  lib\libnet			\
  base

# The list of directories to build the raptor layout engine and
# related libraries.

RAPTOR_DIRS =			\
  htmlparser			\
  dom				\
  gfx				\
  view				\
  widget			\
  layout			\
  webshell

# Main rules

all:: all_dist all_raptor

export:: export_dist export_raptor

libs:: libs_dist libs_raptor

install:: install_dist install_raptor

depend:: depend_dist depend_raptor

clobber:: clobber_dist clobber_raptor
	cd $(MOZ_SRC)\ns
	-rd /s /q dist

clobber_all:: clobber_all_dist clobber_all_raptor
	cd $(MOZ_SRC)\ns
	-rd /s /q dist

######################################################################

# Rule to build subdirectories

$(DIST_DIRS) $(RAPTOR_DIRS)::
!if "$(WINOS)" == "WIN95"
    @echo +++ make: cannot recursively make on win95 using command.com, use w95make.
!else
    @echo +++ make: %MAKE_ARGS% in $(MAKEDIR)\$@
	@cd $@
	@$(NMAKE) -f $(THAT_MAKEFILE) %%MAKE_ARGS%%
    @cd $(MAKEDIR) 
!endif

######################################################################

# Rules for pulling the source from the cvs repository

pull_all: pull_lizard pull_xpcom pull_imglib pull_netlib pull_raptor 

pull_lizard:
	@cd $(MOZ_SRC)\.
	$(CVSCO_LIZARD) ns/config
	$(CVSCO_LIZARD) ns/lib/liblayer
	$(CVSCO_LIZARD) ns/modules/zlib
	$(CVSCO_LIZARD) ns/modules/libutil
	$(CVSCO_LIZARD) ns/nsprpub
	$(CVSCO_LIZARD) ns/sun-java
	$(CVSCO_LIZARD) ns/nav-java
	$(CVSCO_LIZARD) ns/js
	$(CVSCO_LIZARD) ns/modules/security/freenav
	$(CVSCO_LIZARD) ns/modules/libpref

pull_xpcom:
	@cd $(MOZ_SRC)\.
	$(CVSCO_XPCOM) ns/modules/libreg 
	$(CVSCO_XPCOM) ns/xpcom

pull_imglib:
	@cd $(MOZ_SRC)\.
	$(CVSCO_IMGLIB) ns/jpeg
	$(CVSCO_IMGLIB) ns/modules/libutil
	$(CVSCO_IMGLIB) ns/modules/libimg 

pull_netlib:
	@cd $(MOZ_SRC)\.
	$(CVSCO_NETLIB) ns/lib/xp
	$(CVSCO_NETLIB) ns/lib/libnet
	$(CVSCO_NETLIB) ns/include

pull_raptor:
	@cd $(MOZ_SRC)\.
	$(CVSCO_RAPTOR) ns/base
	$(CVSCO_RAPTOR) ns/dom
	$(CVSCO_RAPTOR) ns/gfx
	$(CVSCO_RAPTOR) ns/htmlparser
	$(CVSCO_RAPTOR) ns/layout
	$(CVSCO_RAPTOR) ns/view
	$(CVSCO_RAPTOR) ns/webshell
	$(CVSCO_RAPTOR) ns/widget

######################################################################

# Build rules for the "dist" portion. The "dist" contains those things
# which are imported by the raptor test programs.

all_dist:
    @cd $(MOZ_SRC)\ns
    $(NMAKE) -f $(THIS_MAKEFILE) export_dist
    $(NMAKE) -f $(THIS_MAKEFILE) libs_dist
    $(NMAKE) -f $(THIS_MAKEFILE) install_dist

export_dist::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) export $(MAKEDIR) $(DIST_DIRS)
!else
    @set MAKE_ARGS=export

export_dist:: $(DIST_DIRS)
!endif

libs_dist::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) libs $(MAKEDIR) $(DIST_DIRS)
!else
    @set MAKE_ARGS=libs

libs_dist:: $(DIST_DIRS)
!endif

install_dist::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) install $(MAKEDIR) $(DIST_DIRS)
!else
    @set MAKE_ARGS=install

install_dist:: $(DIST_DIRS)
!endif

depend_dist::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) depend $(MAKEDIR) $(DIST_DIRS)
!else
    @set MAKE_ARGS=depend

depend_dist:: $(DIST_DIRS)
!endif

clobber_dist::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) clobber $(MAKEDIR) $(DIST_DIRS)
!else
    @set MAKE_ARGS=clobber

clobber_dist:: $(DIST_DIRS)
!endif

clobber_all_dist::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) clobber_all $(MAKEDIR) $(DIST_DIRS)
!else
    @set MAKE_ARGS=clobber_all

clobber_all_dist:: $(DIST_DIRS)
!endif

######################################################################

# Build rules for the "raptor" portion. This builds the raptor software
# including the sample webshell viewer application.

all_raptor:
    cd $(MOZ_SRC)\ns
    $(NMAKE) -f $(THIS_MAKEFILE) export_raptor
    $(NMAKE) -f $(THIS_MAKEFILE) libs_raptor
    $(NMAKE) -f $(THIS_MAKEFILE) install_raptor

export_raptor::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) export $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=export

export_raptor:: $(RAPTOR_DIRS)
!endif

libs_raptor::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) libs $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=libs

libs_raptor:: $(RAPTOR_DIRS)
!endif

install_raptor::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) install $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=install

install_raptor:: $(RAPTOR_DIRS)
!endif

depend_raptor::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) depend $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=depend

depend_raptor:: $(RAPTOR_DIRS)
!endif

clobber_raptor::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) clobber $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=clobber

clobber_raptor:: $(RAPTOR_DIRS)
!endif

clobber_all_raptor::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) clobber_all $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=clobber_all

clobber_all_raptor:: $(RAPTOR_DIRS)
!endif

######################################################################

# Build raptor Doc++ documentation
DOCXX = $(MOZ_TOOLS)\bin\docxx
DOCXX_RAPTOR = $(DOCXX) -H -A -p -f -B c:\fake_banner-file_name -j -a
DOCXX_DESTDIR = $(MOZ_SRC)\ns\dist\documentation

doc_raptor:
    -rm -rf $(DOCXX_DESTDIR)
    -@mkdir $(DOCXX_DESTDIR)
    @for %d in (raptor xpcom img dom) do \
      $(DOCXX_RAPTOR) -d $(DOCXX_DESTDIR)\%d $(MOZ_SRC)\ns\dist\public\%d\*.h
    @echo Documentation written to $(DOCXX_DESTDIR)

######################################################################

# Build tarball

TAR = tar
ZIP = $(MOZ_TOOLS)\bin\zip
GZIP = gzip

TARBALL = $(MOZ_SRC)\raptor-win-src-04-15-98.tar
TARBALL_ZIP = $(MOZ_SRC)\raptor-win-src-04-15-98.zip

tarball: prepare_for_tarballing
    @echo Making $(TARBALL)
    cd $(MOZ_SRC)\.
    rm -f $(TARBALL)
    $(TAR) cf $(TARBALL) ns

tarball_zip: prepare_for_tarballing
    @echo Making $(TARBALL_ZIP)
    cd $(MOZ_SRC)\.
    rm -f $(TARBALL_ZIP)
    $(ZIP) -9 -r -q $(TARBALL_ZIP) ns

tarball_gz: $(TARBALL)
    @echo Making gzip of $(TARBALL)
    cd $(MOZ_SRC)\.
    rm -f $(TARBALL).gz
    $(GZIP) -9 -q $(TARBALL)

prepare_for_tarballing:
    $(NMAKE) -f $(THIS_MAKEFILE) clobber clobber_all
