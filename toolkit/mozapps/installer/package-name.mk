# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# assemble package names, see convention at
# http://developer.mozilla.org/index.php?title=En/Package_Filename_Convention
# for (at least Firefox) releases we use a different format with directories,
# e.g. win32/de/Firefox Setup 3.0.1.exe
# the latter format is triggered with MOZ_PKG_PRETTYNAMES=1

ifndef PACKAGE_NAME_MK_INCLUDED
PACKAGE_NAME_MK_INCLUDED := 1

ifndef MOZ_PKG_VERSION
MOZ_PKG_VERSION = $(MOZ_APP_VERSION)
endif

ifndef MOZ_PKG_PLATFORM
MOZ_PKG_PLATFORM := $(TARGET_OS)-$(TARGET_CPU)

# TARGET_OS/TARGET_CPU may be unintuitive, so we hardcode some special formats
ifeq ($(OS_ARCH),WINNT)
ifeq ($(TARGET_CPU),x86_64)
MOZ_PKG_PLATFORM := win64-$(TARGET_CPU)
else
MOZ_PKG_PLATFORM := win32
endif
endif
ifeq ($(OS_ARCH),Darwin)
ifdef UNIVERSAL_BINARY
MOZ_PKG_PLATFORM := mac
else
ifeq ($(TARGET_CPU),x86_64)
MOZ_PKG_PLATFORM := mac64
else
MOZ_PKG_PLATFORM := mac
endif
endif
endif
ifeq ($(TARGET_OS),linux-gnu)
MOZ_PKG_PLATFORM := linux-$(TARGET_CPU)
endif
endif #MOZ_PKG_PLATFORM

ifdef MOZ_PKG_SPECIAL
MOZ_PKG_PLATFORM := $(MOZ_PKG_PLATFORM)-$(MOZ_PKG_SPECIAL)
endif

MOZ_PKG_DIR = $(MOZ_APP_NAME)

ifndef MOZ_PKG_PRETTYNAMES # standard package names

ifndef MOZ_PKG_APPNAME
MOZ_PKG_APPNAME = $(MOZ_APP_NAME)
endif

PKG_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).$(AB_CD).$(MOZ_PKG_PLATFORM)
PKG_PATH =
PKG_INST_BASENAME = $(PKG_BASENAME).installer
PKG_STUB_BASENAME = $(PKG_BASENAME).installer-stub
PKG_INST_PATH = install/sea/
PKG_UPDATE_BASENAME = $(PKG_BASENAME)
CHECKSUMS_FILE_BASENAME = $(PKG_BASENAME)
MOZ_INFO_BASENAME = $(PKG_BASENAME)
PKG_UPDATE_PATH = update/
COMPLETE_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).complete.mar
# PARTIAL_MAR needs to be processed by $(wildcard) before you use it.
PARTIAL_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).partial.*.mar
PKG_LANGPACK_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).$(AB_CD).langpack
PKG_LANGPACK_PATH = $(MOZ_PKG_PLATFORM)/xpi/
LANGPACK = $(PKG_LANGPACK_PATH)$(PKG_LANGPACK_BASENAME).xpi
PKG_SRCPACK_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).source
PKG_BUNDLE_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION)
PKG_SRCPACK_PATH =

else # "pretty" release package names

ifndef MOZ_PKG_APPNAME
MOZ_PKG_APPNAME = $(MOZ_APP_DISPLAYNAME)
endif
MOZ_PKG_APPNAME_LC = $(shell echo $(MOZ_PKG_APPNAME) | tr '[A-Z]' '[a-z]')

ifndef MOZ_PKG_LONGVERSION
MOZ_PKG_LONGVERSION = $(MOZ_PKG_VERSION)
endif

ifeq (,$(filter-out Darwin, $(OS_ARCH))) # Mac
PKG_BASENAME = $(MOZ_PKG_APPNAME) $(MOZ_PKG_LONGVERSION)
PKG_INST_BASENAME = $(MOZ_PKG_APPNAME) Setup $(MOZ_PKG_LONGVERSION)
else
ifeq (,$(filter-out WINNT, $(OS_ARCH))) # Windows
PKG_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
PKG_INST_BASENAME = $(MOZ_PKG_APPNAME) Setup $(MOZ_PKG_LONGVERSION)
PKG_STUB_BASENAME = $(MOZ_PKG_APPNAME) Setup Stub $(MOZ_PKG_LONGVERSION)
else # unix (actually, not Windows, Mac or OS/2)
PKG_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
PKG_INST_BASENAME = $(MOZ_PKG_APPNAME_LC)-setup-$(MOZ_PKG_VERSION)
endif
endif
PKG_PATH = $(MOZ_PKG_PLATFORM)/$(AB_CD)/
CHECKSUMS_FILE_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
MOZ_INFO_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
ifeq ($(MOZ_APP_NAME),xulrunner)
PKG_PATH = runtimes/
PKG_BASENAME = $(MOZ_APP_NAME)-$(MOZ_PKG_VERSION).$(AB_CD).$(MOZ_PKG_PLATFORM)
CHECKSUMS_FILE_BASENAME = $(PKG_BASENAME)
MOZ_INFO_BASENAME = $(PKG_BASENAME)
endif
PKG_INST_PATH = $(PKG_PATH)
PKG_UPDATE_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
PKG_UPDATE_PATH = update/$(PKG_PATH)
COMPLETE_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).complete.mar
# PARTIAL_MAR needs to be processed by $(wildcard) before you use it.
PARTIAL_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).partial.*.mar
PKG_LANGPACK_BASENAME = $(AB_CD)
PKG_LANGPACK_PATH = $(MOZ_PKG_PLATFORM)/xpi/
LANGPACK = $(PKG_LANGPACK_PATH)$(PKG_LANGPACK_BASENAME).xpi
PKG_SRCPACK_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION).source
PKG_BUNDLE_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
PKG_SRCPACK_PATH = source/

endif # MOZ_PKG_PRETTYNAMES

# Symbol package naming
SYMBOL_FULL_ARCHIVE_BASENAME = $(PKG_BASENAME).crashreporter-symbols-full
SYMBOL_ARCHIVE_BASENAME = $(PKG_BASENAME).crashreporter-symbols

# Test package naming
TEST_PACKAGE = $(PKG_BASENAME).tests.zip

ifneq (,$(wildcard $(DIST)/bin/application.ini))
BUILDID = $(shell $(PYTHON) $(MOZILLA_DIR)/config/printconfigsetting.py $(DIST)/bin/application.ini App BuildID)
else
BUILDID = $(shell $(PYTHON) $(MOZILLA_DIR)/config/printconfigsetting.py $(DIST)/bin/platform.ini Build BuildID)
endif

ifndef INCLUDED_RCS_MK
  USE_RCS_MK := 1
  include $(topsrcdir)/config/makefiles/makeutils.mk
endif

MOZ_SOURCE_STAMP = $(firstword $(shell hg -R $(MOZILLA_DIR) parent --template="{node|short}\n" 2>/dev/null))

###########################################################################
# bug: 746277 - preserve existing functionality.
# MOZILLA_DIR="": cd $(SPACE); hg # succeeds if ~/.hg exists
###########################################################################
ifdef MOZILLA_OFFICIAL
MOZ_SOURCE_REPO = $(call getSourceRepo,$(MOZILLA_DIR)$(NULL) $(NULL))
endif

MOZ_SOURCESTAMP_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME).txt
MOZ_BUILDINFO_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME).json

# JavaScript Shell
PKG_JSSHELL = $(DIST)/jsshell-$(MOZ_PKG_PLATFORM).zip

endif # PACKAGE_NAME_MK_INCLUDED
