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

!if !defined(MODULAR_NETLIB) || !defined(STANDALONE_IMAGE_LIB) || !defined(NGLAYOUT_PLUGINS)
#ERR_MSG = ^
#You need to set MODULAR_NETLIB=1, STANDALONE_IMAGE_LIB=1 ^
#and NGLAYOUT_PLUGINS=1 in your environment.
#!ERROR $(ERR_MSG)
set MODULAR_NETLIB=1
set STANDALONE_IMAGE_LIB=1
set NGLAYOUT_PLUGINS=1
!endif

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

!if defined(MOZ_DATE)
CVSCO = cvs -q co -P -D "$(MOZ_DATE)"
!else
CVSCO = cvs -q co -P
!endif

CVSCO_TAG = cvs -q co -P

# Branch tags we use

IMGLIB_BRANCH =
PLUGIN_BRANCH =
XPCOM_BRANCH =

##############################
## all this pull logic now exists in client.mak
## which is the right place for it to be. 
## It should be removed from here as soon as practical
##############################

!if defined(MOZ_DATE)
# CVS commands to pull the appropriate branch versions
CVSCO_LIBPREF = $(CVSCO)
CVSCO_PLUGIN = $(CVSCO)
!else
# CVS commands to pull the appropriate branch versions
CVSCO_LIBPREF = $(CVSCO) -A
CVSCO_PLUGIN = $(CVSCO) -A
!endif

CVSCO_XPCOM = $(CVSCO)
CVSCO_IMGLIB = $(CVSCO)
CVSCO_RAPTOR = $(CVSCO)
CVSCO_LIZARD = $(CVSCO)
CVSCO_NETWORK = $(CVSCO)


# The list of directories that need to be built to build the
# standalone nglayout test program. The order is important. The
# DIST_DIRS need to be built before the RAPTOR_DIRS.

DIST_DIRS =			\
  config			\
  dbm				\
  nsprpub			\
  include			\
  jpeg				\
  modules\libreg		\
  xpcom				\
  modules\zlib			\
  modules\libutil		\
  sun-java			\
  js				\
  modules\security\freenav	\
  modules\libpref		\
  modules\libimg		\
  modules\plugin		\
  modules\oji		\
  base                          \
  intl                          \
  caps                          \
  lib\xp			\
  lib\libpwcac			\
  network			\
!if defined(NGPREFS)
  cmd\wincom			\
  cmd\winfe\prefs		\
!endif
  $(NULL)

# The list of directories to build the nglayout layout engine and
# related libraries.

RAPTOR_DIRS =			\
  expat 			\
  htmlparser			\
  editor 			\
  gfx				\
  dom				\
  view				\
  widget			\
  layout			\
  rdf                           \
  silentdl                      \
  webshell			

# Main rules

all:: all_dist all_nglayout

export:: export_dist export_nglayout

libs:: libs_dist libs_nglayout

install:: install_dist install_nglayout

makedep.exe: $(MOZ_SRC)\mozilla\config\makedep.cpp
        cd $(MOZ_SRC)\mozilla\config
!if "$(WINOS)" != "WINNT"
    @$(W95MAKE) makefile.win export
!else
    $(NMAKE) -f makefile.win export
!endif
        cd $(MOZ_SRC)\$(MOZ_TOP)

depend:: makedep.exe depend_dist depend_nglayout

clobber:: clobber_dist clobber_nglayout
	cd $(MOZ_SRC)\$(MOZ_TOP)
	-rm -r -f dist

clobber_all:: clobber_all_dist clobber_all_nglayout
	cd $(MOZ_SRC)\$(MOZ_TOP)
	-rd /s /q dist
	-rm -r -f dist

browse_info::
	cd $(MOZ_SRC)\$(MOZ_TOP)
	-dir /s /b *.sbr > sbrlist.tmp
	-bscmake /n /o nglayout.bsc @sbrlist.tmp
	-rm sbrlist.tmp

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

##############################
## all this pull logic now exists in client.mak
## which is the right place for it to be. 
## It should be removed from here as soon as practical
##############################

# Rules for pulling the source from the cvs repository

pull_all: pull_seamonkey

pull_nglayout: pull_lizard pull_xpcom pull_imglib pull_netlib pull_nglayoutcore \
pull_editor

pull_seamonkey:
	cd $(MOZ_SRC)\.
#	$(CVSCO_TAG) -A NSPR
	$(CVSCO_LIZARD) SeaMonkeyEditor

pull_lizard:
	cd $(MOZ_SRC)\.
	$(CVSCO_LIZARD) $(MOZ_TOP)/LICENSE
	$(CVSCO_LIZARD) $(MOZ_TOP)/LEGAL
	$(CVSCO_LIZARD) $(MOZ_TOP)/config
	$(CVSCO_LIZARD) $(MOZ_TOP)/dbm
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/zlib
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/libutil
	$(CVSCO_TAG) -r NSPRPUB_RELEASE_3_0 $(MOZ_TOP)/nsprpub
	$(CVSCO_LIZARD) $(MOZ_TOP)/sun-java
	$(CVSCO_LIZARD) $(MOZ_TOP)/nav-java
	$(CVSCO_LIZARD) $(MOZ_TOP)/js
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/security/freenav
	$(CVSCO_LIBPREF) $(MOZ_TOP)/modules/libpref
	$(CVSCO_PLUGIN) $(MOZ_TOP)/modules/plugin
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/oji
	$(CVSCO_LIZARD) $(MOZ_TOP)/caps
        $(CVSCO_LIZARD) $(MOZ_TOP)/rdf
        $(CVSCO_LIZARD) $(MOZ_TOP)/intl
!if defined(NGPREFS)
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/wincom
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/winfe/defaults.h
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/winfe/nsIDefaultBrowser.h
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/winfe/prefs
!endif

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
	$(CVSCO_NETWORK) $(MOZ_TOP)/lib/xp
	$(CVSCO_NETWORK) $(MOZ_TOP)/lib/libpwcac
	$(CVSCO_NETWORK) $(MOZ_TOP)/network
	$(CVSCO_NETWORK) $(MOZ_TOP)/include

pull_nglayoutcore:
	@cd $(MOZ_SRC)\.
	$(CVSCO_RAPTOR) $(MOZ_TOP)/base
	$(CVSCO_RAPTOR) $(MOZ_TOP)/dom
	$(CVSCO_RAPTOR) $(MOZ_TOP)/gfx
	$(CVSCO_RAPTOR) $(MOZ_TOP)/expat
	$(CVSCO_RAPTOR) $(MOZ_TOP)/htmlparser
	$(CVSCO_RAPTOR) $(MOZ_TOP)/layout
	$(CVSCO_RAPTOR) $(MOZ_TOP)/view
	$(CVSCO_RAPTOR) $(MOZ_TOP)/webshell
	$(CVSCO_RAPTOR) $(MOZ_TOP)/widget
	$(CVSCO_RAPTOR) $(MOZ_TOP)/xpfe

pull_editor:
	@cd $(MOZ_SRC)\.
	$(CVSCO_RAPTOR) $(MOZ_TOP)/editor
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

!if exist($(MOZ_TOOLS)\bin\unix_date.exe)
DATE_CMD=$(MOZ_TOOLS)\bin\unix_date.exe
!else
DATE_CMD=$(MOZ_TOOLS)\bin\date.exe
!endif

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

TARBALL = $(MOZ_SRC)\mozilla-newlayout-$(DATE)-win32.tar
TARBALL_ZIP = $(MOZ_SRC)\mozilla-newlayout-$(DATE)-win32.zip

TARFILES = mozilla

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
