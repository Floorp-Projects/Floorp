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
# The Original Code is Mozilla toolkit packaging scripts.
#
# The Initial Developer of the Original Code is
# Benjamin Smedberg <bsmedberg@covad.net>
# 
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Robert Kaiser <kairo@kairo.at>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

# assemble package names, see convention at
# http://developer.mozilla.org/index.php?title=En/Package_Filename_Convention
# for (at least Firefox) releases we use a different format with directories,
# e.g. win32/de/Firefox Setup 3.0.1.exe
# the latter format is triggered with MOZ_PKG_PRETTYNAMES=1

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
MOZ_PKG_PLATFORM := mac64
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
ifeq ($(OS_ARCH),OS2)
MOZ_PKG_PLATFORM := os2
endif
ifeq ($(OS_ARCH),BeOS)
ifeq (,$(filter-out 6.%, $(OS_RELEASE)))
MOZ_PKG_PLATFORM := Zeta
else
ifeq (,$(filter-out 5.1, $(OS_RELEASE)))
MOZ_PKG_PLATFORM := BeOS-bone
else
ifeq (,$(filter-out 5.0.4, $(OS_RELEASE)))
MOZ_PKG_PLATFORM := BeOS-bone
else
ifeq (,$(filter-out 5.0, $(OS_RELEASE)))
MOZ_PKG_PLATFORM := BeOS-net_server
else
MOZ_PKG_PLATFORM := BeOS-$(OS_RELEASE)
endif # 5.0
endif # 5.0.4
endif # 5.1
endif # 6.
endif # OS_ARCH BeOS
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
PKG_INST_PATH = install/sea/
PKG_UPDATE_BASENAME = $(PKG_BASENAME)
PKG_UPDATE_PATH = update/
COMPLETE_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).complete.mar
# PARTIAL_MAR needs to be processed by $(wildcard) before you use it.
PARTIAL_MAR = $(PKG_UPDATE_PATH)$(PKG_UPDATE_BASENAME).partial.*.mar
PKG_LANGPACK_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).$(AB_CD).langpack
PKG_LANGPACK_PATH = install/
LANGPACK = $(PKG_LANGPACK_PATH)$(PKG_LANGPACK_BASENAME).xpi
PKG_SRCPACK_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).source
PKG_SRCPACK_PATH =

else # "pretty" release package names

ifndef MOZ_PKG_APPNAME
MOZ_PKG_APPNAME = $(MOZ_APP_DISPLAYNAME)
endif
MOZ_PKG_APPNAME_LC = $(shell echo $(MOZ_PKG_APPNAME) | tr '[A-Z]' '[a-z]')


ifndef MOZ_PKG_LONGVERSION
MOZ_PKG_LONGVERSION = $(shell echo $(MOZ_PKG_VERSION) |\
                       sed -e 's/a\([0-9][0-9]*\)$$/ Alpha \1/' |\
                       sed -e 's/b\([0-9][0-9]*\)$$/ Beta \1/' |\
                       sed -e 's/rc\([0-9][0-9]*\)$$/ RC \1/')
endif

ifeq (,$(filter-out Darwin OS2, $(OS_ARCH))) # Mac and OS2
PKG_BASENAME = $(MOZ_PKG_APPNAME) $(MOZ_PKG_LONGVERSION)
PKG_INST_BASENAME = $(MOZ_PKG_APPNAME) Setup $(MOZ_PKG_LONGVERSION)
else
ifeq (,$(filter-out WINNT, $(OS_ARCH))) # Windows
PKG_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
PKG_INST_BASENAME = $(MOZ_PKG_APPNAME) Setup $(MOZ_PKG_LONGVERSION)
else # unix (actually, not Windows, Mac or OS/2)
PKG_BASENAME = $(MOZ_PKG_APPNAME_LC)-$(MOZ_PKG_VERSION)
PKG_INST_BASENAME = $(MOZ_PKG_APPNAME_LC)-setup-$(MOZ_PKG_VERSION)
endif
endif
PKG_PATH = $(MOZ_PKG_PLATFORM)/$(AB_CD)/
ifeq ($(MOZ_APP_NAME),xulrunner)
PKG_PATH = runtimes/
PKG_BASENAME = $(MOZ_APP_NAME)-$(MOZ_PKG_VERSION).$(AB_CD).$(MOZ_PKG_PLATFORM)
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

MOZ_SOURCE_STAMP = $(firstword $(shell hg -R $(topsrcdir) parent --template="{node|short}\n" 2>/dev/null))
