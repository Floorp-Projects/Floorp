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
THIS_MAKEFILE=nglayout.mak
THAT_MAKEFILE=makefile.win

!if !defined(MOZ_TOP)
#enable builds from changed top level directories
MOZ_TOP=mozilla
!endif

include <$(DEPTH)\config\config.mak>

# This section is copied from rules.mak

!if "$(WINOS)" == "WIN95"
W95MAKE=$(MOZ_SRC)\$(MOZ_TOP)\config\w95make.exe
W32OBJS = $(OBJS:.obj=.obj, )
W32LOBJS = $(OBJS: .= +-.)
!endif

######################################################################

# This makefile is designed to make building the nglayout viewer
# application easy.

#
# Command macro defines
#

CVSCO = cvs -q co -P

# Branch tags we use
IMGLIB_BRANCH = MODULAR_IMGLIB_BRANCH
NETLIB_BRANCH = MODULAR_NETLIB_BRANCH
RAPTOR_BRANCH = RAPTOR_BRANCH
# XPCOM is now on the tip, rah, rah.

# CVS commands to pull the appropriate branch versions
CVSCO_XPCOM = $(CVSCO)
CVSCO_IMGLIB = $(CVSCO) -r $(IMGLIB_BRANCH)
CVSCO_NETLIB = $(CVSCO) -r $(NETLIB_BRANCH)
CVSCO_RAPTOR = $(CVSCO)
CVSCO_LIZARD = $(CVSCO)

# The list of directories that need to be built to build the
# standalone nglayout test program. The order is important. The
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
  base                          \
  lib\xp			\
  lib\libnet

# The list of directories to build the nglayout layout engine and
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

all:: all_dist all_nglayout

export:: export_dist export_nglayout

libs:: libs_dist libs_nglayout

install:: install_dist install_nglayout

depend:: depend_dist depend_nglayout

clobber:: clobber_dist clobber_nglayout
	cd $(MOZ_SRC)\$(MOZ_TOP)
	-rd /s /q dist

clobber_all:: clobber_all_dist clobber_all_nglayout
	cd $(MOZ_SRC)\$(MOZ_TOP)
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

pull_all: pull_lizard pull_xpcom pull_imglib pull_netlib pull_nglayout 

pull_lizard:
	cd $(MOZ_SRC)\.
	$(CVSCO_LIZARD) $(MOZ_TOP)/LICENSE
	$(CVSCO_LIZARD) $(MOZ_TOP)/LEGAL
	$(CVSCO_LIZARD) $(MOZ_TOP)/config
	$(CVSCO_LIZARD) $(MOZ_TOP)/lib/liblayer
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/zlib
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/libutil
	$(CVSCO_LIZARD) $(MOZ_TOP)/nsprpub
	$(CVSCO_LIZARD) $(MOZ_TOP)/sun-java
	$(CVSCO_LIZARD) $(MOZ_TOP)/nav-java
	$(CVSCO_LIZARD) $(MOZ_TOP)/js
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/security/freenav
	$(CVSCO_XPCOM) $(MOZ_TOP)/modules/libpref

pull_xpcom:
	@cd $(MOZ_SRC)\.
	$(CVSCO_XPCOM) $(MOZ_TOP)/modules/libreg 
	$(CVSCO_XPCOM) $(MOZ_TOP)/xpcom

pull_imglib:
	@cd $(MOZ_SRC)\.
	$(CVSCO_IMGLIB) $(MOZ_TOP)/jpeg
	$(CVSCO_IMGLIB) $(MOZ_TOP)/modules/libutil
	$(CVSCO_IMGLIB) $(MOZ_TOP)/modules/libimg 

pull_netlib:
	@cd $(MOZ_SRC)\.
	$(CVSCO_NETLIB) $(MOZ_TOP)/lib/xp
	$(CVSCO_NETLIB) $(MOZ_TOP)/lib/libnet
	$(CVSCO_NETLIB) $(MOZ_TOP)/include

pull_nglayout:
	@cd $(MOZ_SRC)\.
	$(CVSCO_RAPTOR) $(MOZ_TOP)/base
	$(CVSCO_RAPTOR) $(MOZ_TOP)/dom
	$(CVSCO_RAPTOR) $(MOZ_TOP)/gfx
	$(CVSCO_RAPTOR) $(MOZ_TOP)/htmlparser
	$(CVSCO_RAPTOR) $(MOZ_TOP)/layout
	$(CVSCO_RAPTOR) $(MOZ_TOP)/view
	$(CVSCO_RAPTOR) $(MOZ_TOP)/webshell
	$(CVSCO_RAPTOR) $(MOZ_TOP)/widget

######################################################################

# Build rules for the "dist" portion. The "dist" contains those things
# which are imported by the nglayout test programs.

all_dist:
    @cd $(MOZ_SRC)\$(MOZ_TOP)
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

# Build rules for the "nglayout" portion. This builds the nglayout software
# including the sample webshell viewer application.

all_nglayout:
    cd $(MOZ_SRC)\$(MOZ_TOP)
    $(NMAKE) -f $(THIS_MAKEFILE) export_nglayout
    $(NMAKE) -f $(THIS_MAKEFILE) libs_nglayout
    $(NMAKE) -f $(THIS_MAKEFILE) install_nglayout

export_nglayout::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) export $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=export

export_nglayout:: $(RAPTOR_DIRS)
!endif

libs_nglayout::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) libs $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=libs

libs_nglayout:: $(RAPTOR_DIRS)
!endif

install_nglayout::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) install $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=install

install_nglayout:: $(RAPTOR_DIRS)
!endif

depend_nglayout::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) depend $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=depend

depend_nglayout:: $(RAPTOR_DIRS)
!endif

clobber_nglayout::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) clobber $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=clobber

clobber_nglayout:: $(RAPTOR_DIRS)
!endif

clobber_all_nglayout::
!if "$(WINOS)" == "WIN95"
    @$(W95MAKE) clobber_all $(MAKEDIR) $(RAPTOR_DIRS)
!else
    @set MAKE_ARGS=clobber_all

clobber_all_nglayout:: $(RAPTOR_DIRS)
!endif

######################################################################

# Build nglayout Doc++ documentation
DOCXX = $(MOZ_TOOLS)\bin\docxx
DOCXX_RAPTOR = $(DOCXX) -H -A -p -f -B c:\fake_banner-file_name -j -a
DOCXX_DESTDIR = $(MOZ_SRC)\$(MOZ_TOP)\dist\documentation

doc_nglayout:
    -rm -rf $(DOCXX_DESTDIR)
    -@mkdir $(DOCXX_DESTDIR)
    @for %d in (nglayout xpcom img dom netlib) do \
      $(DOCXX_RAPTOR) -d $(DOCXX_DESTDIR)\%d $(MOZ_SRC)\$(MOZ_TOP)\dist\public\%d\*.h
    @echo Documentation written to $(DOCXX_DESTDIR)

######################################################################

# Build tarball

DATE_CMD=$(MOZ_TOOLS)\bin\unix_date
!if [$(DATE_CMD) +%Y%m%d > today.inc]
!endif
DATE=\
!include "today.inc"
DATE=$(DATE: =)^

!if [del today.inc]
!endif

TAR = tar
ZIP = $(MOZ_TOOLS)\bin\zip
GZIP = gzip

TARBALL = $(MOZ_SRC)\win-$(DATE).tar
TARBALL_ZIP = $(MOZ_SRC)\win-$(DATE).zip

TARFILES = mozilla README\\nglayout

tarballs: tarball_zip tarball_gz

tarball_zip: prepare_for_tarballing
    @echo Making $(TARBALL_ZIP)
    cd $(MOZ_SRC)\.
    rm -f $(TARBALL_ZIP)
    $(ZIP) -9 -r -q $(TARBALL_ZIP) $(TARFILES)

tarball_gz: prepare_for_tarballing
    @echo Making $(TARBALL)
    cd $(MOZ_SRC)\.
    rm -f $(TARBALL) $(TARBALL).gz
    $(TAR) cf $(TARBALL) $(TARFILES)
    @echo Making gzip of $(TARBALL)
    $(GZIP) -9 -q $(TARBALL)

prepare_for_tarballing:
    $(NMAKE) -f $(THIS_MAKEFILE) clobber clobber_all
