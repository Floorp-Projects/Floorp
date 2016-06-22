# vim:set ts=8 sw=8 sts=8 noet:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Shared makefile that can be used to easily kick off l10n builds
# of Mozilla applications.
# This makefile should be included, and then assumes that the including
# makefile defines the following targets:
# clobber-zip
#   This target should remove all language dependent-files from $(STAGEDIST),
#   depending on $(AB_CD) set to the locale code.
#   $(AB_CD) will be en-US on the initial unpacking of the package
# libs-%
#   This target should call into the various libs targets that this
#   application depends on.
# installer-%
#   This target should list all required targets, a typical rule would be
#	installers-%: clobber-% langpack-% repackage-zip-%
#		@echo "repackaging done"
#   to initially clobber the locale staging area, and then to build the
#   language pack and zip package.
#   Other targets like windows installers might be listed, too, and should
#   be defined in the including makefile.
# The including makefile should provide values for the variables
#   MOZ_APP_VERSION and MOZ_LANGPACK_EID.


run_for_effects := $(shell if test ! -d $(DIST); then $(NSINSTALL) -D $(DIST); fi)

# This makefile uses variable overrides from the libs-% target to
# build non-default locales to non-default dist/ locations. Be aware!

LPROJ_ROOT = $(firstword $(subst -, ,$(AB_CD)))
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifeq (zh-TW,$(AB_CD))
LPROJ_ROOT := $(subst -,_,$(AB_CD))
endif
endif

# These are defaulted to be compatible with the files the wget-en-US target
# pulls. You may override them if you provide your own files. You _must_
# override them when MOZ_PKG_PRETTYNAMES is defined - the defaults will not
# work in that case.
ZIP_IN ?= $(ABS_DIST)/$(PACKAGE)
WIN32_INSTALLER_IN ?= $(ABS_DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe

# Allows overriding the final destination of the repackaged file
ZIP_OUT ?= $(ABS_DIST)/$(PACKAGE)

ACDEFINES += \
	-DAB_CD=$(AB_CD) \
	-DMOZ_LANGPACK_EID=$(MOZ_LANGPACK_EID) \
	-DMOZ_APP_ID='$(MOZ_APP_ID)' \
	-DMOZ_APP_VERSION=$(MOZ_APP_VERSION) \
	-DMOZ_APP_MAXVERSION=$(MOZ_APP_MAXVERSION) \
	-DLOCALE_SRCDIR=$(abspath $(LOCALE_SRCDIR)) \
	-DPKG_BASENAME='$(PKG_BASENAME)' \
	-DPKG_INST_BASENAME='$(PKG_INST_BASENAME)' \
	$(NULL)


clobber-%:
	$(RM) -rf $(DIST)/xpi-stage/locale-$*


PACKAGER_NO_LIBS = 1

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
STAGEDIST = $(ABS_DIST)/l10n-stage/$(MOZ_PKG_DIR)/$(_APPNAME)/Contents/Resources
else
STAGEDIST = $(ABS_DIST)/l10n-stage/$(MOZ_PKG_DIR)
endif

include $(MOZILLA_DIR)/toolkit/mozapps/installer/signing.mk
include $(MOZILLA_DIR)/toolkit/mozapps/installer/packager.mk

PACKAGE_BASE_DIR = $(ABS_DIST)/l10n-stage

$(STAGEDIST): AB_CD:=en-US
$(STAGEDIST): UNPACKAGE=$(call ESCAPE_WILDCARD,$(ZIP_IN))
$(STAGEDIST): $(call ESCAPE_WILDCARD,$(ZIP_IN))
# only mac needs to remove the parent of STAGEDIST...
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
	$(RM) -r -v $(DIST)/l10n-stage
else
# ... and windows doesn't like removing STAGEDIST itself, remove all children
	find $(STAGEDIST) -maxdepth 1 -print0 | xargs -0 $(RM) -r
endif
	$(NSINSTALL) -D $(DIST)/l10n-stage
	cd $(DIST)/l10n-stage && \
	  $(INNER_UNMAKE_PACKAGE)


unpack: $(STAGEDIST)
	@echo done unpacking

# The path to the object dir for the mozilla-central build system,
# may be overridden if necessary.
MOZDEPTH ?= $(DEPTH)

ifdef MOZ_MAKE_COMPLETE_MAR
MAKE_COMPLETE_MAR = 1
ifeq ($(OS_ARCH), WINNT)
ifneq ($(MOZ_PKG_FORMAT), SFX7Z)
MAKE_COMPLETE_MAR =
endif
endif
endif
repackage-zip: UNPACKAGE='$(ZIP_IN)'
repackage-zip:  libs-$(AB_CD)
# call a hook for apps to put their uninstall helper.exe into the package
	$(UNINSTALLER_PACKAGE_HOOK)
# call a hook for apps to build the stub installer
ifdef MOZ_STUB_INSTALLER
	$(STUB_HOOK)
endif
	$(PYTHON) $(MOZILLA_DIR)/toolkit/mozapps/installer/l10n-repack.py $(STAGEDIST) $(DIST)/xpi-stage/locale-$(AB_CD) \
		$(MOZ_PKG_EXTRAL10N) \
		$(if $(filter omni,$(MOZ_PACKAGER_FORMAT)),$(if $(NON_OMNIJAR_FILES),--non-resource $(NON_OMNIJAR_FILES)))

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifneq (en,$(LPROJ_ROOT))
	mv $(STAGEDIST)/en.lproj $(STAGEDIST)/$(LPROJ_ROOT).lproj
endif
ifdef MOZ_CRASHREPORTER
# On Mac OS X, the crashreporter.ini file needs to be moved from under the
# application bundle's Resources directory where all other l10n files are
# located to the crash reporter bundle's Resources directory.
	mv $(STAGEDIST)/crashreporter.app/Contents/Resources/crashreporter.ini \
	  $(STAGEDIST)/../MacOS/crashreporter.app/Contents/Resources/crashreporter.ini
	$(RM) -rf $(STAGEDIST)/crashreporter.app
endif
endif

	$(NSINSTALL) -D $(DIST)/l10n-stage/$(PKG_PATH)
	cd $(DIST)/l10n-stage; \
	  $(MAKE_PACKAGE)
ifdef MAKE_COMPLETE_MAR
	$(MAKE) -C $(MOZDEPTH)/tools/update-packaging full-update AB_CD=$(AB_CD) \
	  MOZ_PKG_PRETTYNAMES=$(MOZ_PKG_PRETTYNAMES) \
	  PACKAGE_BASE_DIR='$(ABS_DIST)/l10n-stage'
endif
# packaging done, undo l10n stuff
ifneq (en,$(LPROJ_ROOT))
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
	mv $(STAGEDIST)/$(LPROJ_ROOT).lproj $(STAGEDIST)/en.lproj
endif
endif
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
	mv -f '$(DIST)/l10n-stage/$(PACKAGE)' '$(ZIP_OUT)'
	if test -f '$(DIST)/l10n-stage/$(PACKAGE).asc'; then mv -f '$(DIST)/l10n-stage/$(PACKAGE).asc' '$(ZIP_OUT).asc'; fi

repackage-zip-%: $(STAGEDIST)
	@$(MAKE) repackage-zip AB_CD=$* ZIP_IN='$(ZIP_IN)'

APP_DEFINES = $(firstword $(wildcard $(LOCALE_SRCDIR)/defines.inc) \
                          $(srcdir)/en-US/defines.inc)
TK_DEFINES = $(firstword \
   $(wildcard $(call EXPAND_LOCALE_SRCDIR,toolkit/locales)/defines.inc) \
   $(MOZILLA_DIR)/toolkit/locales/en-US/defines.inc)

# Dealing with app sub dirs: If DIST_SUBDIRS is defined it contains a
# listing of app sub-dirs we should include in langpack xpis. If not,
# check DIST_SUBDIR, and if that isn't present, just package the default
# chrome directory.
PKG_ZIP_DIRS = chrome $(or $(DIST_SUBDIRS),$(DIST_SUBDIR))

langpack-%: LANGPACK_FILE=$(ABS_DIST)/$(PKG_LANGPACK_PATH)$(PKG_LANGPACK_BASENAME).xpi
langpack-%: AB_CD=$*
langpack-%: XPI_NAME=locale-$*
langpack-%: libs-%
	@echo 'Making langpack $(LANGPACK_FILE)'
	$(NSINSTALL) -D $(DIST)/$(PKG_LANGPACK_PATH)
	$(call py_action,preprocessor,$(DEFINES) $(ACDEFINES) \
	  -DTK_DEFINES=$(TK_DEFINES) -DAPP_DEFINES=$(APP_DEFINES) $(MOZILLA_DIR)/toolkit/locales/generic/install.rdf -o $(DIST)/xpi-stage/$(XPI_NAME)/install.rdf)
	$(call py_action,zip,-C $(DIST)/xpi-stage/locale-$(AB_CD) $(LANGPACK_FILE) install.rdf $(PKG_ZIP_DIRS) chrome.manifest)

# This variable is to allow the wget-en-US target to know which ftp server to download from
ifndef EN_US_BINARY_URL 
EN_US_BINARY_URL = $(error You must set EN_US_BINARY_URL)
endif

# This make target allows us to wget the latest en-US binary from a specified website
# The make installers-% target needs the en-US binary in dist/
# and for the windows repackages we need the .installer.exe in dist/sea
wget-en-US:
ifndef WGET
	$(error Wget not installed)
endif
	$(NSINSTALL) -D $(ABS_DIST)/$(PKG_PATH)
	(cd $(ABS_DIST)/$(PKG_PATH) && $(WGET) --no-cache -nv --no-iri -N  '$(EN_US_BINARY_URL)/$(PACKAGE)')
	@echo 'Downloaded $(EN_US_BINARY_URL)/$(PACKAGE) to $(ABS_DIST)/$(PKG_PATH)/$(PACKAGE)'
ifdef RETRIEVE_WINDOWS_INSTALLER
ifeq ($(OS_ARCH), WINNT)
	$(NSINSTALL) -D $(ABS_DIST)/$(PKG_INST_PATH)
	(cd $(ABS_DIST)/$(PKG_INST_PATH) && $(WGET) --no-cache -nv --no-iri -N '$(EN_US_BINARY_URL)/$(PKG_PATH)$(PKG_INST_BASENAME).exe')
	@echo 'Downloaded $(EN_US_BINARY_URL)/$(PKG_PATH)$(PKG_INST_BASENAME).exe to $(ABS_DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe'
endif
endif

generate-snippet-%:
	$(PYTHON) $(MOZILLA_DIR)/tools/update-packaging/generatesnippet.py \
          --mar-path=$(ABS_DIST)/update \
          --application-ini-file=$(STAGEDIST)/application.ini \
          --locale=$* \
          --product=$(MOZ_PKG_APPNAME) \
          --platform=$(MOZ_PKG_PLATFORM) \
          --download-base-URL=$(DOWNLOAD_BASE_URL) \
          --verbose
