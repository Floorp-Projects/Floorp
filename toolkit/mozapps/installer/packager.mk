# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Benjamin Smedberg <bsmedberg@covad.net>
#   Arthur Wiebe <artooro@gmail.com>
#   Mark Mentovai <mark@moxienet.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

include $(topsrcdir)/toolkit/mozapps/installer/package-name.mk

# This is how we create the Unix binary packages we release to the public.
# Currently the only format is tar.gz (TGZ), but it should be fairly easy
# to add .rpm (RPM) and .deb (DEB) later.

ifndef MOZ_PKG_FORMAT
ifneq (,$(filter mac cocoa,$(MOZ_WIDGET_TOOLKIT)))
MOZ_PKG_FORMAT  = DMG
else
ifeq (,$(filter-out OS2 WINNT, $(OS_ARCH)))
MOZ_PKG_FORMAT  = ZIP
INSTALLER_DIR   = windows
else
ifeq (,$(filter-out SunOS, $(OS_ARCH)))
MOZ_PKG_FORMAT  = BZ2
else
MOZ_PKG_FORMAT  = TGZ
endif
INSTALLER_DIR   = unix
endif
endif
endif # MOZ_PKG_FORMAT

PACKAGE       = $(PKG_BASENAME)$(PKG_SUFFIX)

MAKE_PACKAGE	= $(error What is a $(MOZ_PKG_FORMAT) package format?);

CREATE_FINAL_TAR = tar -c --owner=0 --group=0 --numeric-owner --mode="go-w" -f
UNPACK_TAR       = tar -x

ifeq ($(MOZ_PKG_FORMAT),TAR)
PKG_SUFFIX	= .tar
MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_APPNAME) > $(PACKAGE)
UNMAKE_PACKAGE	= $(UNPACK_TAR) < $(UNPACKAGE)
endif
ifeq ($(MOZ_PKG_FORMAT),TGZ)
PKG_SUFFIX	= .tar.gz
MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_APPNAME) | gzip -vf9 > $(PACKAGE)
UNMAKE_PACKAGE	= gunzip -c $(UNPACKAGE) | $(UNPACK_TAR)
endif
ifeq ($(MOZ_PKG_FORMAT),BZ2)
PKG_SUFFIX	= .tar.bz2
MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_APPNAME) | bzip2 -vf > $(PACKAGE)
UNMAKE_PACKAGE	= bunzip2 -c $(UNPACKAGE) | $(UNPACK_TAR)
endif
ifeq ($(MOZ_PKG_FORMAT),ZIP)
PKG_SUFFIX	= .zip
MAKE_PACKAGE	= $(ZIP) -r9D $(PACKAGE) $(MOZ_PKG_APPNAME)
UNMAKE_PACKAGE	= $(UNZIP) $(UNPACKAGE)
endif
ifeq ($(MOZ_PKG_FORMAT),DMG)
ifndef _APPNAME
ifdef MOZ_DEBUG
_APPNAME	= $(MOZ_APP_DISPLAYNAME)Debug.app
else
_APPNAME	= $(MOZ_APP_DISPLAYNAME).app
endif
endif
PKG_SUFFIX	= .dmg
PKG_DMG_FLAGS	=
ifneq (,$(MOZ_PKG_MAC_DSSTORE))
PKG_DMG_FLAGS += --copy "$(MOZ_PKG_MAC_DSSTORE):/.DS_Store"
endif
ifneq (,$(MOZ_PKG_MAC_BACKGROUND))
PKG_DMG_FLAGS += --mkdir /.background --copy "$(MOZ_PKG_MAC_BACKGROUND):/.background"
endif
ifneq (,$(MOZ_PKG_MAC_ICON))
PKG_DMG_FLAGS += --icon "$(MOZ_PKG_MAC_ICON)"
endif
ifneq (,$(MOZ_PKG_MAC_RSRC))
PKG_DMG_FLAGS += --resource "$(MOZ_PKG_MAC_RSRC)"
endif
ifneq (,$(MOZ_PKG_MAC_EXTRA))
PKG_DMG_FLAGS += $(MOZ_PKG_MAC_EXTRA)
endif
_ABS_TOPSRCDIR = $(shell cd $(topsrcdir) && pwd)
MAKE_PACKAGE	= $(_ABS_TOPSRCDIR)/build/package/mac_osx/pkg-dmg \
  --source "$(MOZ_PKG_APPNAME)" --target "$(PACKAGE)" \
  --volname "$(MOZ_APP_DISPLAYNAME)" $(PKG_DMG_FLAGS)
UNMAKE_PACKAGE	= \
  set -ex; \
  function cleanup() { \
    hdiutil detach $${DEV_NAME} || \
     (sleep 5 && hdiutil detach $${DEV_NAME} -force); \
    return $$1 && $$?; \
  }; \
  unset NEXT_ROOT; \
  export PAGER=true; \
  echo Y | hdiutil attach -readonly -mountroot /tmp -private -noautoopen $(UNPACKAGE) > hdi.output; \
  DEV_NAME=`perl -n -e 'if($$_=~/(\/dev\/disk[^ ]*)/) {print $$1."\n";exit;}'< hdi.output`; \
  MOUNTPOINT=`perl -n -e 'split(/\/dev\/disk[^ ]*/,$$_,2);if($$_[1]=~/(\/.*)/) {print $$1."\n";exit;}'< hdi.output` || cleanup 1; \
  rsync -a "$${MOUNTPOINT}/$(_APPNAME)" $(MOZ_PKG_APPNAME) || cleanup 1; \
  test -n "$(MOZ_PKG_MAC_DSSTORE)" && \
    (rsync -a "$${MOUNTPOINT}/.DS_Store" "$(MOZ_PKG_MAC_DSSTORE)" || cleanup 1); \
  test -n "$(MOZ_PKG_MAC_BACKGROUND)" && \
    (rsync -a "$${MOUNTPOINT}/.background/`basename "$(MOZ_PKG_MAC_BACKGROUND)"`" "$(MOZ_PKG_MAC_BACKGROUND)" || cleanup 1); \
  test -n "$(MOZ_PKG_MAC_ICON)" && \
    (rsync -a "$${MOUNTPOINT}/.VolumeIcon.icns" "$(MOZ_PKG_MAC_ICON)" || cleanup 1); \
  cleanup 0; \
  if test -n "$(MOZ_PKG_MAC_RSRC)" ; then \
    hdiutil unflatten $(UNPACKAGE) && \
    (/Developer/Tools/DeRez -skip plst -skip blkx $(UNPACKAGE) > "$(MOZ_PKG_MAC_RSRC)" || (hdiutil flatten $(UNPACKAGE) && false)) && \
    hdiutil flatten $(UNPACKAGE); \
  fi; \
  $(NULL)
# The plst and blkx resources are skipped because they belong to each
# individual dmg and are created by hdiutil.
endif

# dummy macro if we don't have PSM built
SIGN_NSS		=
ifndef CROSS_COMPILE
ifdef MOZ_PSM
SIGN_NSS		= @echo signing nss libraries;

SIGN_CMD	= $(DIST)/bin/run-mozilla.sh $(DIST)/bin/shlibsign -v -i

SOFTOKN		= $(DIST)/$(MOZ_PKG_APPNAME)/$(DLL_PREFIX)softokn3$(DLL_SUFFIX)
FREEBL_HYBRID	= $(DIST)/$(MOZ_PKG_APPNAME)/$(DLL_PREFIX)freebl_hybrid_3$(DLL_SUFFIX)
FREEBL_PURE	= $(DIST)/$(MOZ_PKG_APPNAME)/$(DLL_PREFIX)freebl_pure32_3$(DLL_SUFFIX)

SIGN_NSS	+= $(SIGN_CMD) $(SOFTOKN); \
        if test -f $(FREEBL_HYBRID); then $(SIGN_CMD) $(FREEBL_HYBRID); fi; \
        if test -f $(FREEBL_PURE); then $(SIGN_CMD) $(FREEBL_PURE); fi;

endif # MOZ_PSM
endif # !CROSS_COMPILE

NO_PKG_FILES += \
	core \
	bsdecho \
	gtscc \
	jscpucfg \
	nsinstall \
	viewer \
	TestGtkEmbed \
	bloaturls.txt \
	codesighs* \
	elf-dynstr-gc \
	mangle* \
	maptsv* \
	mfc* \
	mkdepend* \
	msdump* \
	msmap* \
	nm2tsv* \
	nsinstall* \
	rebasedlls* \
	res/samples \
	res/throbber \
	shlibsign* \
	winEmbed.exe \
	chrome/chrome.rdf \
	chrome/app-chrome.manifest \
	chrome/overlayinfo \
	components/compreg.dat \
	components/xpti.dat \
	$(NULL)

# browser/locales/Makefile uses this makefile for it's variable defs, but
# doesn't want the libs:: rule.
ifndef PACKAGER_NO_LIBS
libs:: $(PACKAGE)
endif

GARBAGE		+= $(DIST)/$(PACKAGE) $(PACKAGE)

ifdef USE_SHORT_LIBNAME
MOZILLA_BIN	= $(DIST)/bin/$(MOZ_PKG_APPNAME)$(BIN_SUFFIX)
else
MOZILLA_BIN	= $(DIST)/bin/$(MOZ_PKG_APPNAME)-bin
endif

ifeq ($(OS_ARCH),IRIX)
STRIP_FLAGS	= -f
endif
ifeq ($(OS_ARCH),BeOS)
STRIP_FLAGS	= -g
PLATFORM_EXCLUDE_LIST = ! -name "*.stub" ! -name "$(MOZ_PKG_APPNAME)-bin"
endif
ifeq ($(OS_ARCH),OS2)
STRIP		= $(srcdir)/os2/strip.cmd
STRIP_FLAGS	=
PLATFORM_EXCLUDE_LIST = ! -name "*.ico"
endif

ifneq (,$(filter WINNT OS2,$(OS_ARCH)))
PKGCP_OS = dos
else
PKGCP_OS = unix
endif

$(PACKAGE): $(MOZILLA_BIN) $(MOZ_PKG_MANIFEST)
	@rm -rf $(DIST)/$(MOZ_PKG_APPNAME) $(DIST)/$(PKG_BASENAME).tar $(DIST)/$(PKG_BASENAME).dmg $@ $(EXCLUDE_LIST)
# NOTE: this must be a tar now that dist links into the tree so that we
# do not strip the binaries actually in the tree.
	@echo "Creating package directory..."
	@mkdir $(DIST)/$(MOZ_PKG_APPNAME)
ifdef MOZ_PKG_MANIFEST
	$(RM) -rf $(DIST)/xpt
	$(PERL) -I$(topsrcdir)/xpinstall/packager -e 'use Packager; \
	  Packager::Copy("$(DIST)", "$(DIST)/$(MOZ_PKG_APPNAME)", \
	                 "$(MOZ_PKG_MANIFEST)", "$(PKGCP_OS)", 1, 0, 1);'
	$(PERL) $(topsrcdir)/xpinstall/packager/xptlink.pl -s $(DIST) -d $(DIST)/xpt -f $(DIST)/$(MOZ_PKG_APPNAME)/components -v -o $(PKGCP_OS)
else # !MOZ_PKG_MANIFEST
ifeq ($(MOZ_PKG_FORMAT),DMG)
	@cd $(DIST) && rsync -auv --copy-unsafe-links $(_APPNAME) $(MOZ_PKG_APPNAME)
else
	@cd $(DIST)/bin && tar $(TAR_CREATE_FLAGS) - * | (cd ../$(MOZ_PKG_APPNAME); tar -xf -)
endif # DMG
endif # MOZ_PKG_MANIFEST
ifndef PKG_SKIP_STRIP
	@echo "Stripping package directory..."
	@cd $(DIST)/$(MOZ_PKG_APPNAME); find . ! -type d \
			! -name "*.js" \
			! -name "*.xpt" \
			! -name "*.gif" \
			! -name "*.jpg" \
			! -name "*.png" \
			! -name "*.xpm" \
			! -name "*.txt" \
			! -name "*.rdf" \
			! -name "*.sh" \
			! -name "*.properties" \
			! -name "*.dtd" \
			! -name "*.html" \
			! -name "*.xul" \
			! -name "*.css" \
			! -name "*.xml" \
			! -name "*.jar" \
			! -name "*.dat" \
			! -name "*.tbl" \
			! -name "*.src" \
			! -name "*.reg" \
			$(PLATFORM_EXCLUDE_LIST) \
			-exec $(STRIP) $(STRIP_FLAGS) {} >/dev/null 2>&1 \;
	$(SIGN_NSS)
endif
	@echo "Removing unpackaged files..."
ifeq ($(MOZ_PKG_FORMAT),DMG)
	cd $(DIST)/$(MOZ_PKG_APPNAME)/$(_APPNAME)/Contents/MacOS; rm -rf $(NO_PKG_FILES)
else
	cd $(DIST)/$(MOZ_PKG_APPNAME); rm -rf $(NO_PKG_FILES)
endif
	@echo "Compressing..."
	cd $(DIST); $(MAKE_PACKAGE)
