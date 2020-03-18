# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# assemble package names, see convention at
# http://developer.mozilla.org/index.php?title=En/Package_Filename_Convention
# Note that release packages are named during the post-build release
# automation, so they aren't part of this file.

ifndef PACKAGE_NAME_MK_INCLUDED
PACKAGE_NAME_MK_INCLUDED := 1

ifndef MOZ_PKG_VERSION
MOZ_PKG_VERSION = $(MOZ_APP_VERSION)
endif

ifndef MOZ_PKG_PLATFORM
MOZ_PKG_PLATFORM := $(TARGET_OS)-$(TARGET_CPU)

ifeq ($(MOZ_BUILD_APP),mobile/android)
MOZ_PKG_PLATFORM := android-$(TARGET_CPU)
endif

# TARGET_OS/TARGET_CPU may be unintuitive, so we hardcode some special formats
ifeq ($(OS_ARCH),WINNT)
ifeq ($(CPU_ARCH),x86)
MOZ_PKG_PLATFORM := win32
else
ifeq ($(CPU_ARCH),aarch64)
MOZ_PKG_PLATFORM := win64-aarch64
else
MOZ_PKG_PLATFORM := win64
endif
endif
endif
ifeq ($(OS_ARCH),Darwin)
MOZ_PKG_PLATFORM := mac
endif
ifeq ($(TARGET_OS),linux-gnu)
MOZ_PKG_PLATFORM := linux-$(TARGET_CPU)
endif
endif #MOZ_PKG_PLATFORM

ifdef MOZ_PKG_SPECIAL
MOZ_PKG_PLATFORM := $(MOZ_PKG_PLATFORM)-$(MOZ_PKG_SPECIAL)
endif

MOZ_PKG_DIR ?= $(MOZ_APP_NAME)

ifndef MOZ_PKG_APPNAME
MOZ_PKG_APPNAME = $(MOZ_APP_NAME)
endif

ifdef MOZ_SIMPLE_PACKAGE_NAME
PKG_BASENAME := $(MOZ_SIMPLE_PACKAGE_NAME)
else
PKG_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).$(AB_CD).$(MOZ_PKG_PLATFORM)
endif
PKG_PATH =
SDK_PATH =
PKG_INST_BASENAME = $(PKG_BASENAME).installer
PKG_STUB_BASENAME = $(PKG_BASENAME).installer-stub
PKG_INST_PATH = install/sea/
PKG_UPDATE_BASENAME = $(PKG_BASENAME)
CHECKSUMS_FILE_BASENAME = $(PKG_BASENAME)
MOZ_INFO_BASENAME = $(PKG_BASENAME)
PKG_UPDATE_PATH = update/
COMPLETE_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).complete.mar
ifdef MOZ_SIMPLE_PACKAGE_NAME
PKG_LANGPACK_BASENAME = $(MOZ_SIMPLE_PACKAGE_NAME).langpack
PKG_LANGPACK_PATH =
else
PKG_LANGPACK_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).$(AB_CD).langpack
PKG_LANGPACK_PATH = $(MOZ_PKG_PLATFORM)/xpi/
endif
LANGPACK = $(PKG_LANGPACK_PATH)$(PKG_LANGPACK_BASENAME).xpi
PKG_SRCPACK_BASENAME = source
PKG_BUNDLE_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION)
PKG_SRCPACK_PATH =

# Symbol package naming
SYMBOL_FULL_ARCHIVE_BASENAME = $(PKG_BASENAME).crashreporter-symbols-full
SYMBOL_ARCHIVE_BASENAME = $(PKG_BASENAME).crashreporter-symbols

# Generated file package naming
GENERATED_SOURCE_FILE_PACKAGE = $(PKG_BASENAME).generated-files.tar.gz

# Code coverage package naming
CODE_COVERAGE_ARCHIVE_BASENAME = $(PKG_BASENAME).code-coverage-gcno

# Mozsearch package naming
MOZSEARCH_ARCHIVE_BASENAME = $(PKG_BASENAME).mozsearch-index
MOZSEARCH_RUST_ANALYSIS_BASENAME = $(PKG_BASENAME).mozsearch-rust
MOZSEARCH_RUST_STDLIB_BASENAME = $(PKG_BASENAME).mozsearch-rust-stdlib
MOZSEARCH_INCLUDEMAP_BASENAME = $(PKG_BASENAME).mozsearch-distinclude

# Mozharness naming
MOZHARNESS_PACKAGE = mozharness.zip

# Test package naming
TEST_PACKAGE = $(PKG_BASENAME).common.tests.tar.gz
CPP_TEST_PACKAGE = $(PKG_BASENAME).cppunittest.tests.tar.gz
XPC_TEST_PACKAGE = $(PKG_BASENAME).xpcshell.tests.tar.gz
MOCHITEST_PACKAGE = $(PKG_BASENAME).mochitest.tests.tar.gz
REFTEST_PACKAGE = $(PKG_BASENAME).reftest.tests.tar.gz
WP_TEST_PACKAGE = $(PKG_BASENAME).web-platform.tests.tar.gz
TALOS_PACKAGE = $(PKG_BASENAME).talos.tests.tar.gz
AWSY_PACKAGE = $(PKG_BASENAME).awsy.tests.tar.gz
GTEST_PACKAGE = $(PKG_BASENAME).gtest.tests.tar.gz

# macOS codesigning package naming
MACOS_CODESIGN_ARCHIVE_BASENAME = $(PKG_BASENAME).codesign-entitlements

ifneq (,$(wildcard $(DIST)/bin/application.ini))
BUILDID = $(shell $(PYTHON3) $(MOZILLA_DIR)/config/printconfigsetting.py $(DIST)/bin/application.ini App BuildID)
else
BUILDID = $(shell $(PYTHON3) $(MOZILLA_DIR)/config/printconfigsetting.py $(DIST)/bin/platform.ini Build BuildID)
endif

MOZ_SOURCESTAMP_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME).txt
MOZ_BUILDINFO_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME).json
MOZ_BUILDHUB_JSON = $(DIST)/$(PKG_PATH)/buildhub.json
MOZ_NORMANDY_JSON = $(DIST)/$(PKG_PATH)/$(PKG_BASENAME).normandy.json
MOZ_BUILDID_INFO_TXT_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME)_info.txt
MOZ_MOZINFO_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME).mozinfo.json
MOZ_TEST_PACKAGES_FILE = $(DIST)/$(PKG_PATH)/$(PKG_BASENAME).test_packages.json

# JavaScript Shell
ifdef MOZ_SIMPLE_PACKAGE_NAME
JSSHELL_NAME := $(MOZ_SIMPLE_PACKAGE_NAME).jsshell.zip
else
JSSHELL_NAME = jsshell-$(MOZ_PKG_PLATFORM).zip
endif
PKG_JSSHELL = $(DIST)/$(JSSHELL_NAME)

endif # PACKAGE_NAME_MK_INCLUDED
