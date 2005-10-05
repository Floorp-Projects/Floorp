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

ifndef MOZ_PKG_APPNAME
MOZ_PKG_APPNAME = $(MOZ_APP_NAME)
endif

ifndef MOZ_PKG_VERSION
MOZ_PKG_VERSION = $(MOZ_APP_VERSION)
endif

ifndef MOZ_PKG_PLATFORM
MOZ_PKG_PLATFORM := $(TARGET_OS)-$(TARGET_CPU)

# TARGET_OS/TARGET_CPU may be unintuitive, so we hardcode some special formats
ifeq ($(OS_ARCH),WINNT)
MOZ_PKG_PLATFORM := win$(MOZ_BITS)
endif
ifeq ($(OS_ARCH),Darwin)
MOZ_PKG_PLATFORM := mac
endif
ifeq ($(TARGET_OS),linux-gnu)
MOZ_PKG_PLATFORM := linux-$(TARGET_CPU)
endif
ifeq ($(OS_ARCH),OS2)
MOZ_PKG_PLATFORM := os2
endif

# GTK2 is the default, so we mark gtk1 builds
ifeq ($(MOZ_WIDGET_TOOLKIT),gtk)
MOZ_PKG_PLATFORM := $(MOZ_PKG_PLATFORM)-gtk1
endif
endif #MOZ_PKG_PLATFORM

PKG_BASENAME = $(MOZ_PKG_APPNAME)-$(MOZ_PKG_VERSION).$(AB_CD).$(MOZ_PKG_PLATFORM)
