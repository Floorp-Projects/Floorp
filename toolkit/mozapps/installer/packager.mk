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
ifeq ($(OS_ARCH),OS2)
INSTALLER_DIR   = os2
else
INSTALLER_DIR   = windows
endif
else
ifeq (,$(filter-out SunOS, $(OS_ARCH)))
MOZ_PKG_FORMAT  = BZ2
else
ifeq ($(MOZ_WIDGET_TOOLKIT),gtk2)
MOZ_PKG_FORMAT  = BZ2
else
MOZ_PKG_FORMAT  = TGZ
endif
endif
INSTALLER_DIR   = unix
endif
endif
endif # MOZ_PKG_FORMAT

PACKAGE       = $(PKG_BASENAME)$(PKG_SUFFIX)

MAKE_PACKAGE	= $(error What is a $(MOZ_PKG_FORMAT) package format?);

CREATE_FINAL_TAR = $(TAR) -c --owner=0 --group=0 --numeric-owner \
  --mode="go-w" -f
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
ifndef _BINPATH
_BINPATH	= /$(_APPNAME)/Contents/MacOS
endif # _BINPATH
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
ifdef UNIVERSAL_BINARY
STAGEPATH = universal/
endif
ifndef PKG_DMG_SOURCE
PKG_DMG_SOURCE = $(STAGEPATH)$(MOZ_PKG_APPNAME)
endif
MAKE_PACKAGE	= $(_ABS_TOPSRCDIR)/build/package/mac_osx/pkg-dmg \
  --source "$(PKG_DMG_SOURCE)" --target "$(PACKAGE)" \
  --volname "$(MOZ_APP_DISPLAYNAME)" $(PKG_DMG_FLAGS)
UNMAKE_PACKAGE	= \
  set -ex; \
  function cleanup() { \
    hdiutil detach $${DEV_NAME} || \
     { sleep 5 && hdiutil detach $${DEV_NAME} -force; }; \
    return $$1 && $$?; \
  }; \
  unset NEXT_ROOT; \
  export PAGER=true; \
  echo Y | hdiutil attach -readonly -mountroot /tmp -private -noautoopen $(UNPACKAGE) > hdi.output; \
  DEV_NAME=`perl -n -e 'if($$_=~/(\/dev\/disk[^ ]*)/) {print $$1."\n";exit;}'< hdi.output`; \
  MOUNTPOINT=`perl -n -e 'split(/\/dev\/disk[^ ]*/,$$_,2);if($$_[1]=~/(\/.*)/) {print $$1."\n";exit;}'< hdi.output` || cleanup 1; \
  rsync -a "$${MOUNTPOINT}/$(_APPNAME)" $(MOZ_PKG_APPNAME) || cleanup 1; \
  test -n "$(MOZ_PKG_MAC_DSSTORE)" && \
    { rsync -a "$${MOUNTPOINT}/.DS_Store" "$(MOZ_PKG_MAC_DSSTORE)" || cleanup 1; }; \
  test -n "$(MOZ_PKG_MAC_BACKGROUND)" && \
    { rsync -a "$${MOUNTPOINT}/.background/`basename "$(MOZ_PKG_MAC_BACKGROUND)"`" "$(MOZ_PKG_MAC_BACKGROUND)" || cleanup 1; }; \
  test -n "$(MOZ_PKG_MAC_ICON)" && \
    { rsync -a "$${MOUNTPOINT}/.VolumeIcon.icns" "$(MOZ_PKG_MAC_ICON)" || cleanup 1; }; \
  cleanup 0; \
  if test -n "$(MOZ_PKG_MAC_RSRC)" ; then \
    cp $(UNPACKAGE) $(MOZ_PKG_APPNAME).tmp.dmg && \
    hdiutil unflatten $(MOZ_PKG_APPNAME).tmp.dmg && \
    { /Developer/Tools/DeRez -skip plst -skip blkx $(MOZ_PKG_APPNAME).tmp.dmg > "$(MOZ_PKG_MAC_RSRC)" || { rm -f $(MOZ_PKG_APPNAME).tmp.dmg && false; }; } && \
    rm -f $(MOZ_PKG_APPNAME).tmp.dmg; \
  fi; \
  $(NULL)
# The plst and blkx resources are skipped because they belong to each
# individual dmg and are created by hdiutil.
endif

# dummy macro if we don't have PSM built
SIGN_NSS		=
ifneq (1_,$(if $(CROSS_COMPILE),1,0)_$(UNIVERSAL_BINARY))
ifdef MOZ_PSM
SIGN_NSS		= @echo signing nss libraries;

ifdef UNIVERSAL_BINARY
NATIVE_ARCH	= $(shell uname -p | sed -e s/powerpc/ppc/)
NATIVE_DIST	= $(DIST)/../../$(NATIVE_ARCH)/dist
SIGN_CMD	= $(NATIVE_DIST)/bin/run-mozilla.sh $(NATIVE_DIST)/bin/shlibsign -v -i
else
SIGN_CMD	= $(RUN_TEST_PROGRAM) $(DIST)/bin/shlibsign -v -i
endif

SOFTOKN		= $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)/$(DLL_PREFIX)softokn3$(DLL_SUFFIX)
FREEBL		= $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)/$(DLL_PREFIX)freebl3$(DLL_SUFFIX)
FREEBL_32FPU	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)/$(DLL_PREFIX)freebl_32fpu_3$(DLL_SUFFIX)
FREEBL_32INT	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)/$(DLL_PREFIX)freebl_32int_3$(DLL_SUFFIX)
FREEBL_32INT64	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)/$(DLL_PREFIX)freebl_32int64_3$(DLL_SUFFIX)
FREEBL_64FPU	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)/$(DLL_PREFIX)freebl_64fpu_3$(DLL_SUFFIX)
FREEBL_64INT	= $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)/$(DLL_PREFIX)freebl_64int_3$(DLL_SUFFIX)

SIGN_NSS	+= $(SIGN_CMD) $(SOFTOKN); \
	if test -f $(FREEBL); then $(SIGN_CMD) $(FREEBL); fi; \
	if test -f $(FREEBL_32FPU); then $(SIGN_CMD) $(FREEBL_32FPU); fi; \
	if test -f $(FREEBL_32INT); then $(SIGN_CMD) $(FREEBL_32INT); fi; \
	if test -f $(FREEBL_32INT64); then $(SIGN_CMD) $(FREEBL_32INT64); fi; \
	if test -f $(FREEBL_64FPU); then $(SIGN_CMD) $(FREEBL_64FPU); fi; \
	if test -f $(FREEBL_64INT); then $(SIGN_CMD) $(FREEBL_64INT); fi;

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
	os2Embed.exe \
	chrome/chrome.rdf \
	chrome/app-chrome.manifest \
	chrome/overlayinfo \
	components/compreg.dat \
	components/xpti.dat \
	content_unit_tests \
	necko_unit_tests \
	$(NULL)

# browser/locales/Makefile uses this makefile for it's variable defs, but
# doesn't want the libs:: rule.
ifndef PACKAGER_NO_LIBS
libs:: make-package
endif

DEFINES += -DDLL_PREFIX=$(DLL_PREFIX) -DDLL_SUFFIX=$(DLL_SUFFIX)

ifdef MOZ_PKG_REMOVALS
MOZ_PKG_REMOVALS_GEN = removed-files

$(MOZ_PKG_REMOVALS_GEN): $(MOZ_PKG_REMOVALS) Makefile Makefile.in
	$(PYTHON) $(topsrcdir)/config/Preprocessor.py -Fsubstitution $(DEFINES) $(ACDEFINES) $(MOZ_PKG_REMOVALS) > $(MOZ_PKG_REMOVALS_GEN)
endif

GARBAGE		+= $(DIST)/$(PACKAGE) $(PACKAGE)

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

# The following target stages files into three directories: one directory for
# locale-independent files, one for locale-specific files, and one for optional
# extensions based on the information in the MOZ_PKG_MANIFEST file and the
# following vars:
# MOZ_NONLOCALIZED_PKG_LIST
# MOZ_LOCALIZED_PKG_LIST
# MOZ_OPTIONAL_PKG_LIST

PKG_ARG = , "$(pkg)"

# Define packager macro to work around make 3.81 backslash issue (bug #339933)
define PACKAGER_COPY
$(PERL) -I$(topsrcdir)/xpinstall/packager -e 'use Packager; \
       Packager::Copy($1,$2,$3,$4,$5,$6,$7);'
endef

installer-stage: $(MOZ_PKG_MANIFEST)
ifndef MOZ_PKG_MANIFEST
	$(error MOZ_PKG_MANIFEST unspecified!)
endif
	@rm -rf $(DEPTH)/installer-stage $(DIST)/xpt
	@echo "Staging installer files..."
	@$(NSINSTALL) -D $(DEPTH)/installer-stage/nonlocalized
	@$(NSINSTALL) -D $(DEPTH)/installer-stage/localized
	@$(NSINSTALL) -D $(DEPTH)/installer-stage/optional
	@$(NSINSTALL) -D $(DIST)/xpt
	$(call PACKAGER_COPY, "$(DIST)",\
		"$(DEPTH)/installer-stage/nonlocalized", \
		"$(MOZ_PKG_MANIFEST)", "$(PKGCP_OS)", 1, 0, 1 \
		$(foreach pkg,$(MOZ_NONLOCALIZED_PKG_LIST),$(PKG_ARG)) )
	$(call PACKAGER_COPY, "$(DIST)",\
		"$(DEPTH)/installer-stage/localized", \
		"$(MOZ_PKG_MANIFEST)", "$(PKGCP_OS)", 1, 0, 1 \
		$(foreach pkg,$(MOZ_LOCALIZED_PKG_LIST),$(PKG_ARG)) )
	$(call PACKAGER_COPY, "$(DIST)",\
		"$(DEPTH)/installer-stage/optional", \
		"$(MOZ_PKG_MANIFEST)", "$(PKGCP_OS)", 1, 0, 1 \
		$(foreach pkg,$(MOZ_OPTIONAL_PKG_LIST),$(PKG_ARG)) )
	$(PERL) $(topsrcdir)/xpinstall/packager/xptlink.pl -s $(DIST) -d $(DIST)/xpt -f $(DEPTH)/installer-stage/nonlocalized/components -v

stage-package: $(MOZ_PKG_MANIFEST) $(MOZ_PKG_REMOVALS_GEN)
	@rm -rf $(DIST)/$(MOZ_PKG_APPNAME) $(DIST)/$(PKG_BASENAME).tar $(DIST)/$(PKG_BASENAME).dmg $@ $(EXCLUDE_LIST)
# NOTE: this must be a tar now that dist links into the tree so that we
# do not strip the binaries actually in the tree.
	@echo "Creating package directory..."
	@mkdir $(DIST)/$(MOZ_PKG_APPNAME)
ifdef MOZ_PKG_MANIFEST
	$(RM) -rf $(DIST)/xpt
	$(call PACKAGER_COPY, "$(DIST)",\
		 "$(DIST)/$(MOZ_PKG_APPNAME)", \
		"$(MOZ_PKG_MANIFEST)", "$(PKGCP_OS)", 1, 0, 1)
	$(PERL) $(topsrcdir)/xpinstall/packager/xptlink.pl -s $(DIST) -d $(DIST)/xpt -f $(DIST)/$(MOZ_PKG_APPNAME)/components -v
else # !MOZ_PKG_MANIFEST
ifeq ($(MOZ_PKG_FORMAT),DMG)
# If UNIVERSAL_BINARY, the package will be made from an already-prepared
# STAGEPATH
ifndef UNIVERSAL_BINARY
	@cd $(DIST) && rsync -auv --copy-unsafe-links $(_APPNAME) $(MOZ_PKG_APPNAME)
endif
else
	@cd $(DIST)/bin && tar $(TAR_CREATE_FLAGS) - * | (cd ../$(MOZ_PKG_APPNAME); tar -xf -)
endif # DMG
endif # MOZ_PKG_MANIFEST
ifndef PKG_SKIP_STRIP
	@echo "Stripping package directory..."
	@cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME); find . ! -type d \
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
ifdef NO_PKG_FILES
	cd $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH); rm -rf $(NO_PKG_FILES)
endif
ifdef MOZ_PKG_REMOVALS
	$(SYSINSTALL) $(MOZ_PKG_REMOVALS_GEN) $(DIST)/$(STAGEPATH)$(MOZ_PKG_APPNAME)$(_BINPATH)
endif # MOZ_PKG_REMOVALS

make-package: stage-package
	@echo "Compressing..."
	cd $(DIST); $(MAKE_PACKAGE)
