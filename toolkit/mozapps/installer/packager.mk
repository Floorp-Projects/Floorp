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

TAR_CREATE_FLAGS = -cvhf

ifeq ($(OS_ARCH),BSD_OS)
TAR_CREATE_FLAGS = -cvLf
endif

CREATE_FINAL_TAR = tar -c --owner=0 --group=0 --numeric-owner --mode="go-w" -f

ifeq ($(MOZ_PKG_FORMAT),TAR)
PKG_SUFFIX	= .tar
MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_APPNAME) > $(PACKAGE)
endif
ifeq ($(MOZ_PKG_FORMAT),TGZ)
PKG_SUFFIX	= .tar.gz
MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_APPNAME) | gzip -vf9 > $(PACKAGE)
endif
ifeq ($(MOZ_PKG_FORMAT),BZ2)
PKG_SUFFIX	= .tar.bz2
MAKE_PACKAGE 	= $(CREATE_FINAL_TAR) - $(MOZ_PKG_APPNAME) | bzip2 -vf > $(PACKAGE)
endif
ifeq ($(MOZ_PKG_FORMAT),ZIP)
PKG_SUFFIX	= .zip
MAKE_PACKAGE	= $(ZIP) -r9D $(PACKAGE) $(MOZ_PKG_APPNAME)
endif
ifeq ($(MOZ_PKG_FORMAT),DMG)
ifdef MOZ_DEBUG
_APPNAME	= $(MOZ_APP_DISPLAYNAME)Debug.app
else
_APPNAME	= $(MOZ_APP_DISPLAYNAME).app
endif
PKG_SUFFIX	= .dmg.gz
_ABS_TOPSRCDIR	= $(shell cd $(topsrcdir) && pwd)
MAKE_PACKAGE	= $(_ABS_TOPSRCDIR)/build/package/mac_osx/make-diskimage $(PKG_BASENAME).dmg $(MOZ_PKG_APPNAME) $(MOZ_APP_DISPLAYNAME) && gzip -vf9 $(PKG_BASENAME).dmg
endif

# dummy macro if we don't have PSM built
SIGN_NSS		=
ifndef CROSS_COMPILE
ifdef MOZ_PSM
SIGN_NSS		= @echo signing nss libraries;

SIGN_ENV	= LD_LIBRARY_PATH=$(DIST)/bin:${LD_LIBRARY_PATH} \
		LD_LIBRARYN32_PATH=$(DIST)/bin:${LD_LIBRARYN32_PATH} \
		LD_LIBRARYN64_PATH=$(DIST)/bin:${LD_LIBRARYN64_PATH} \
		LD_LIBRARY_PATH_64=$(DIST)/bin:${LD_LIBRARY_PATH_64} \
		SHLIB_PATH=$(DIST)/bin:${SHLIB_PATH} LIBPATH=$(DIST)/bin:${LIBPATH} \
		DYLD_LIBRARY_PATH=$(DIST)/bin:${DYLD_LIBRARY_PATH} \
		LIBRARY_PATH=$(DIST)/bin:${LIBRARY_PATH}

SIGN_CMD	= cd $(DIST)/$(MOZ_PKG_APPNAME) && $(SIGN_ENV) $(DIST)/bin/shlibsign -v -i

SOFTOKN		= $(DIST)/$(MOZ_PKG_APPNAME)/$(DLL_PREFIX)softokn3$(DLL_SUFFIX)
FREEBL_HYBRID	= $(DIST)/$(MOZ_PKG_APPNAME)/$(DLL_PREFIX)freebl_hybrid_3$(DLL_SUFFIX)
FREEBL_PURE	= $(DIST)/$(MOZ_PKG_APPNAME)/$(DLL_PREFIX)freebl_pure32_3$(DLL_SUFFIX)

SIGN_NSS	+= $(SIGN_CMD) $(SOFTOKN); \
        if test -f $(FREEBL_HYBRID); then $(SIGN_CMD) $(FREEBL_HYBRID); fi; \
        if test -f $(FREEBL_PURE); then $(SIGN_CMD) $(FREEBL_PURE); fi;

endif # MOZ_PSM
endif # !CROSS_COMPILE

NSPR_LDIR	= $(findstring -L,$(NSPR_LIBS))
ifneq ($(NSPR_LDIR),)
NSPR_LDIR	= $(subst -L,,$(word 1,$(NSPR_LIBS)))
endif

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
	$(NULL)

libs:: $(PACKAGE)

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
TAR_CREATE_FLAGS = -cvf
PLATFORM_EXCLUDE_LIST = ! -name "*.ico"
endif

$(PACKAGE): $(MOZILLA_BIN)
	@rm -rf $(DIST)/$(MOZ_PKG_APPNAME) $(DIST)/$(PKG_BASENAME).tar $(DIST)/$(PKG_BASENAME).dmg $(DIST)/$(PKG_BASENAME).dmg.gz $@ $(EXCLUDE_LIST)
# NOTE: this must be a tar now that dist links into the tree so that we
# do not strip the binaries actually in the tree.
	@echo "Creating package directory..."
	@mkdir $(DIST)/$(MOZ_PKG_APPNAME)
ifeq ($(MOZ_PKG_FORMAT),DMG)
	@cd $(DIST) && rsync -auvL $(_APPNAME) $(MOZ_PKG_APPNAME)
else
	@cd $(DIST)/bin && tar $(TAR_CREATE_FLAGS) - * | (cd ../$(MOZ_PKG_APPNAME); tar -xf -)
ifdef MOZ_NATIVE_NSPR
ifndef EXCLUDE_NSPR_LIBS
	@echo "Copying NSPR libs..."
	@cp -p $(NSPR_LDIR)/*$(DLL_SUFFIX) $(DIST)/$(MOZ_PKG_APPNAME)
	@chmod 755 $(DIST)/$(MOZ_PKG_APPNAME)/*$(DLL_SUFFIX)
endif
endif
endif # DMG
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
	@echo "Removing unpackaged files..."
ifeq ($(MOZ_PKG_FORMAT),DMG)
	cd $(DIST)/$(MOZ_PKG_APPNAME)/$(_APPNAME)/Contents/MacOS; rm -rf $(NO_PKG_FILES)
else
	cd $(DIST)/$(MOZ_PKG_APPNAME); rm -rf $(NO_PKG_FILES)
endif
	@echo "Compressing..."
	cd $(DIST); $(MAKE_PACKAGE)
