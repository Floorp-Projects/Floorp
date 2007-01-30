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
# The Original Code is Mozilla Firefox build scripts.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation <http://www.mozilla.org>.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Robert Strong <rstrong@mozilla.com> - Initial perl scripts (install_sub.pl)
#   Benjamin Smedberg <benjamin@smedbergs.us> - Makefile-izing
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

ifndef CONFIG_DIR
$(error CONFIG_DIR must be set before including makensis.mk)
endif

ABS_CONFIG_DIR := $(shell pwd)/$(CONFIG_DIR)

SFX_MODULE ?= $(error SFX_MODULE is not defined)

TOOLKIT_NSIS_FILES = \
	common.nsh \
	locales.nsi \
	nsProcess.dll \
	overrides.nsh \
	ShellLink.dll \
	version.nsh \
	$(NULL)

$(CONFIG_DIR)/setup.exe::
	$(INSTALL) $(addprefix $(topsrcdir)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(topsrcdir)/toolkit/mozapps/installer/windows/nsis/setup.ico $(CONFIG_DIR)
	cd $(CONFIG_DIR) && makensis.exe installer.nsi
# Support for building the uninstaller when repackaging locales
ifeq ($(CONFIG_DIR),l10ngen)
	cd $(CONFIG_DIR) && makensis.exe uninstaller.nsi
endif

$(CONFIG_DIR)/7zSD.sfx:
	$(CYGWIN_WRAPPER) upx --best -o $(CONFIG_DIR)/7zSD.sfx $(SFX_MODULE)

installer::
	$(INSTALL) $(CONFIG_DIR)/removed-files.log $(CONFIG_DIR)/setup.exe $(DEPTH)/installer-stage
	cd $(DEPTH)/installer-stage && $(CYGWIN_WRAPPER) 7z a -r -t7z $(ABS_CONFIG_DIR)/app.7z -mx -m0=BCJ2 -m1=LZMA:d24 -m2=LZMA:d19 -m3=LZMA:d19  -mb0:1 -mb0s1:2 -mb0s2:3
	$(MAKE) $(CONFIG_DIR)/7zSD.sfx
	$(NSINSTALL) -D $(DIST)/install/sea
	cat $(CONFIG_DIR)/7zSD.sfx $(CONFIG_DIR)/app.tag $(CONFIG_DIR)/app.7z > $(DIST)/install/sea/$(PKG_BASENAME).installer.exe
	chmod 0755 $(DIST)/install/sea/$(PKG_BASENAME).installer.exe

# For building the uninstaller during the application build so it can be
# included for mar file generation.
uninstaller::
	$(INSTALL) $(addprefix $(topsrcdir)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(topsrcdir)/toolkit/mozapps/installer/windows/nsis/setup.ico $(CONFIG_DIR)
	cd $(CONFIG_DIR) && makensis.exe uninstaller.nsi
	$(NSINSTALL) -D $(DIST)/bin/uninstall
ifdef MOZ_PHOENIX
	cp $(CONFIG_DIR)/helper.exe $(DIST)/bin/uninstall
else
	cp $(CONFIG_DIR)/uninst.exe $(DIST)/bin/uninstall
endif
