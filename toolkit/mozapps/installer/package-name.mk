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
ifeq ($(TARGET_CPU),x86_64)
MOZ_PKG_PLATFORM := win64
else
MOZ_PKG_PLATFORM := win32
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
# PARTIAL_MAR needs to be processed by $(wildcard) before you use it.
PARTIAL_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).partial.*.mar
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

# Mozharness naming
MOZHARNESS_PACKAGE = mozharness.zip

# Test package naming
TEST_PACKAGE = $(PKG_BASENAME).common.tests.zip
CPP_TEST_PACKAGE = $(PKG_BASENAME).cppunittest.tests.zip
XPC_TEST_PACKAGE = $(PKG_BASENAME).xpcshell.tests.zip
MOCHITEST_PACKAGE = $(PKG_BASENAME).mochitest.tests.zip
REFTEST_PACKAGE = $(PKG_BASENAME).reftest.tests.zip
WP_TEST_PACKAGE = $(PKG_BASENAME).web-platform.tests.tar.gz
TALOS_PACKAGE = $(PKG_BASENAME).talos.tests.zip
AWSY_PACKAGE = $(PKG_BASENAME).awsy.tests.zip
GTEST_PACKAGE = $(PKG_BASENAME).gtest.tests.zip

ifneq (,$(wildcard $(DIST)/bin/application.ini))
BUILDID = $(shell $(PYTHON) $(MOZILLA_DIR)/config/printconfigsetting.py $(DIST)/bin/application.ini App BuildID)
else
BUILDID = $(shell $(PYTHON) $(MOZILLA_DIR)/config/printconfigsetting.py $(DIST)/bin/platform.ini Build BuildID)
endif

MOZ_SOURCESTAMP_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME).txt
MOZ_BUILDINFO_FILE = $(DIST)/$(PKG_PATH)/$(MOZ_INFO_BASENAME).json
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
