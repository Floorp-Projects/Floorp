# vim:set ts=8 sw=8 sts=8 noet:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Shared makefile that can be used to easily kick off l10n builds
# of Mozilla applications.
# This makefile should be included, and then assumes that the including
# makefile defines the following targets:
# l10n-%
#   This target should call into the various l10n targets that this
#   application depends on.
# installer-%
#   This target should list all required targets, a typical rule would be
#	installers-%: clobber-% langpack-% repackage-zip-%
#		@echo "repackaging done"
#   to initially clobber the locale staging area, and then to build the
#   language pack and zip package.
#   Other targets like windows installers might be listed, too, and should
#   be defined in the including makefile.
#   The installer-% targets should not set AB_CD, so that the unpackaging
#   step finds the original package.
# The including makefile should provide values for the variables
#   MOZ_APP_VERSION and MOZ_LANGPACK_EID.


run_for_effects := $(shell if test ! -d $(DIST); then $(NSINSTALL) -D $(DIST); fi)

# This makefile uses variable overrides from the l10n-% target to
# build non-default locales to non-default dist/ locations. Be aware!

LPROJ_ROOT = $(firstword $(subst -, ,$(AB_CD)))
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifeq (zh-TW,$(AB_CD))
LPROJ_ROOT := $(subst -,_,$(AB_CD))
endif
endif

# These are defaulted to be compatible with the files the wget-en-US target
# pulls. You may override them if you provide your own files.
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

# export some global defines for l10n repacks
BASE_MERGE:=$(CURDIR)/merge-dir
export REAL_LOCALE_MERGEDIR=$(BASE_MERGE)/$(AB_CD)
# is an l10n repack step:
export IS_LANGUAGE_REPACK
# is a language pack:
export IS_LANGPACK

clobber-%: AB_CD=$*
clobber-%:
	$(RM) -rf $(DIST)/xpi-stage/locale-$*


PACKAGER_NO_LIBS = 1

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
STAGEDIST = $(ABS_DIST)/l10n-stage/$(MOZ_PKG_DIR)/$(_APPNAME)/Contents/Resources
else
STAGEDIST = $(ABS_DIST)/l10n-stage/$(MOZ_PKG_DIR)
endif
UNPACKED_INSTALLER = $(ABS_DIST)/unpacked-installer

include $(MOZILLA_DIR)/toolkit/mozapps/installer/packager.mk

PACKAGE_BASE_DIR = $(ABS_DIST)/l10n-stage

$(UNPACKED_INSTALLER): AB_CD:=en-US
$(UNPACKED_INSTALLER): UNPACKAGE=$(call ESCAPE_WILDCARD,$(ZIP_IN))
$(UNPACKED_INSTALLER): $(call ESCAPE_WILDCARD,$(ZIP_IN))
# only mac needs to remove the parent of STAGEDIST...
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
	$(RM) -r -v $(UNPACKED_INSTALLER)
else
# ... and windows doesn't like removing STAGEDIST itself, remove all children
	find $(UNPACKED_INSTALLER) -maxdepth 1 -print0 | xargs -0 $(RM) -r
endif
	$(NSINSTALL) -D $(UNPACKED_INSTALLER)
	$(call INNER_UNMAKE_PACKAGE,$(UNPACKED_INSTALLER))

unpack: $(UNPACKED_INSTALLER)
ifeq ($(OS_ARCH), WINNT)
	$(RM) -r -f $(ABS_DIST)/l10n-stage
	$(NSINSTALL) -D $(ABS_DIST)/l10n-stage
	$(call copy_dir, $(UNPACKED_INSTALLER), $(ABS_DIST)/l10n-stage)
else
	rsync -rav --delete $(UNPACKED_INSTALLER)/ $(ABS_DIST)/l10n-stage
endif

# The path to the object dir for the mozilla-central build system,
# may be overridden if necessary.
MOZDEPTH ?= $(DEPTH)

repackage-zip: UNPACKAGE='$(ZIP_IN)'
repackage-zip:
	$(PYTHON3) $(MOZILLA_DIR)/toolkit/mozapps/installer/l10n-repack.py '$(STAGEDIST)' $(DIST)/xpi-stage/locale-$(AB_CD) \
		$(MOZ_PKG_EXTRAL10N) \
		$(if $(MOZ_PACKAGER_MINIFY),--minify) \
		$(if $(filter omni,$(MOZ_PACKAGER_FORMAT)),$(if $(NON_OMNIJAR_FILES),--non-resource $(NON_OMNIJAR_FILES)))

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
ifneq (en,$(LPROJ_ROOT))
	mv '$(STAGEDIST)'/en.lproj '$(STAGEDIST)'/$(LPROJ_ROOT).lproj
endif
ifdef MOZ_CRASHREPORTER
# On Mac OS X, the crashreporter.ini file needs to be moved from under the
# application bundle's Resources directory where all other l10n files are
# located to the crash reporter bundle's Resources directory.
	mv '$(STAGEDIST)'/crashreporter.app/Contents/Resources/crashreporter.ini \
	  '$(STAGEDIST)'/../MacOS/crashreporter.app/Contents/Resources/crashreporter.ini
	$(RM) -rf '$(STAGEDIST)'/crashreporter.app
endif
endif
ifeq (WINNT,$(OS_ARCH))
	$(MAKE) -C ../installer/windows CONFIG_DIR=l10ngen l10ngen/helper.exe
	cp ../installer/windows/l10ngen/helper.exe $(STAGEDIST)/uninstall
endif

	$(NSINSTALL) -D $(DIST)/l10n-stage/$(PKG_PATH)
	$(call MAKE_PACKAGE,$(DIST)/l10n-stage)
# packaging done, undo l10n stuff
ifneq (en,$(LPROJ_ROOT))
ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
	mv '$(STAGEDIST)'/$(LPROJ_ROOT).lproj '$(STAGEDIST)'/en.lproj
endif
endif
	$(NSINSTALL) -D $(DIST)/$(PKG_PATH)
	mv -f '$(DIST)/l10n-stage/$(PACKAGE)' '$(ZIP_OUT)'
	if test -f '$(DIST)/l10n-stage/$(PACKAGE).asc'; then mv -f '$(DIST)/l10n-stage/$(PACKAGE).asc' '$(ZIP_OUT).asc'; fi

repackage-zip-%: unpack
	@$(MAKE) repackage-zip AB_CD=$* ZIP_IN='$(ZIP_IN)'

# Dealing with app sub dirs: If DIST_SUBDIRS is defined it contains a
# listing of app sub-dirs we should include in langpack xpis. If not,
# check DIST_SUBDIR, and if that isn't present, just package the default
# chrome directory and top-level localization for Fluent.
PKG_ZIP_DIRS = chrome localization $(or $(DIST_SUBDIRS),$(DIST_SUBDIR))

# Clone a l10n repository, either via hg or git
# Make this a variable as it's embedded in a sh conditional
ifeq ($(VCS_CHECKOUT_TYPE),hg)
L10N_CO = $(HG) --cwd $(L10NBASEDIR) clone https://hg.mozilla.org/l10n-central/$(AB_CD)/
else
ifeq ($(VCS_CHECKOUT_TYPE),git)
L10N_CO = $(GIT) -C $(L10NBASEDIR) clone hg://hg.mozilla.org/l10n-central/$(AB_CD)/
else
L10N_CO = $(error You need to use either hg or git)
endif
endif

merge-%: IS_LANGUAGE_REPACK=1
merge-%: AB_CD=$*
merge-%:
# For nightly builds, we automatically check out missing localizations
# from l10n-central.  We never automatically check out in automation:
# automation builds check out revisions that have been signed-off by
# l10n drivers prior to use.
ifdef MOZ_AUTOMATION
	if  ! test -d $(L10NBASEDIR)/$(AB_CD) ; then \
		echo 'Error: Automation requires l10n repositories to be checked out: $(L10NBASEDIR)/$(AB_CD)' ; \
		exit 1 ; \
	fi
endif
ifdef NIGHTLY_BUILD
	if  ! test -d $(L10NBASEDIR)/$(AB_CD) ; then \
		echo 'Checking out $(L10NBASEDIR)/$(AB_CD)' ; \
		$(NSINSTALL) -D $(L10NBASEDIR) ; \
		$(L10N_CO) ; \
	fi
endif
	$(RM) -rf $(REAL_LOCALE_MERGEDIR)
	-$(PYTHON3) $(MOZILLA_DIR)/mach compare-locales --merge $(BASE_MERGE) $(srcdir)/l10n.toml $(L10NBASEDIR) $*
# Hunspell dictionaries are interesting, as we don't ship the en-US
# dictionary in repacks. Thus we can't use the merge logic from
# compare-locales above, which would add en-US.dic and en-US.aff to
# the merge directory.
# Copy them to the merge dir, if exist. The repackaged app can still decide
# on whether to package them or not in `l10n-%` and `chrome-%`.
	if  test -d $(L10NBASEDIR)/$(AB_CD)/extensions/spellcheck ; then \
		$(NSINSTALL) -D $(REAL_LOCALE_MERGEDIR)/extensions/spellcheck/hunspell ; \
		cp $(L10NBASEDIR)/$(AB_CD)/extensions/spellcheck/hunspell/*.* $(REAL_LOCALE_MERGEDIR)/extensions/spellcheck/hunspell ; \
	fi

LANGPACK_METADATA = $(LOCALE_SRCDIR)/langpack-metadata.ftl

langpack-%: IS_LANGUAGE_REPACK=1
langpack-%: IS_LANGPACK=1
langpack-%: AB_CD=$*
langpack-%: clobber-%
	@echo 'Making langpack $(LANGPACK_FILE)'
	@$(MAKE) l10n-$(AB_CD)
	@$(MAKE) package-langpack-$(AB_CD)

package-langpack-%: LANGPACK_FILE=$(ABS_DIST)/$(PKG_LANGPACK_PATH)$(PKG_LANGPACK_BASENAME).xpi
package-langpack-%: XPI_NAME=locale-$*
package-langpack-%: AB_CD=$*
package-langpack-%:
	$(NSINSTALL) -D $(DIST)/$(PKG_LANGPACK_PATH)
	$(call py_action,langpack_manifest $(AB_CD),--locales $(AB_CD) --app-version $(MOZ_APP_VERSION) --max-app-ver $(MOZ_APP_MAXVERSION) --app-name '$(MOZ_APP_DISPLAYNAME)' --l10n-basedir '$(L10NBASEDIR)' --metadata $(LANGPACK_METADATA) --langpack-eid '$(MOZ_LANGPACK_EID)' --input $(DIST)/xpi-stage/locale-$(AB_CD))
	$(call py_action,zip $(PKG_LANGPACK_BASENAME).xpi,-C $(DIST)/xpi-stage/locale-$(AB_CD) -x **/*.manifest -x **/*.js -x **/*.ini $(LANGPACK_FILE) $(PKG_ZIP_DIRS) manifest.json)

# This variable is to allow the wget-en-US target to know which ftp server to download from
ifndef EN_US_BINARY_URL 
EN_US_BINARY_URL = $(error You must set EN_US_BINARY_URL)
endif

# Allow the overriding of PACKAGE format so we can get an EN_US build with a different
# PACKAGE format than we are creating l10n packages with.
EN_US_PACKAGE_NAME ?= $(PACKAGE)

# This make target allows us to wget the latest en-US binary from a specified website
# The make installers-% target needs the en-US binary in dist/
# and for the windows repackages we need the .installer.exe in dist/sea
wget-en-US:
ifndef WGET
	$(error Wget not installed)
endif
	$(NSINSTALL) -D $(ABS_DIST)/$(PKG_PATH)
	(cd $(ABS_DIST)/$(PKG_PATH) && \
        $(WGET) --no-cache -nv --no-iri -N -O $(PACKAGE) '$(EN_US_BINARY_URL)/$(EN_US_PACKAGE_NAME)')
	@echo 'Downloaded $(EN_US_BINARY_URL)/$(EN_US_PACKAGE_NAME) to $(ABS_DIST)/$(PKG_PATH)/$(PACKAGE)'
