# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef CONFIG_DIR
$(error CONFIG_DIR must be set before including makensis.mk)
endif

include $(MOZILLA_DIR)/toolkit/mozapps/installer/signing.mk

ABS_CONFIG_DIR := $(shell pwd)/$(CONFIG_DIR)

SFX_MODULE ?= $(error SFX_MODULE is not defined)

TOOLKIT_NSIS_FILES = \
	common.nsh \
	locales.nsi \
	overrides.nsh \
	setup.ico \
	version.nsh \
	$(NULL)

CUSTOM_NSIS_PLUGINS = \
	AccessControl.dll \
	AppAssocReg.dll \
	ApplicationID.dll \
	CityHash.dll \
	InvokeShellVerb.dll \
	ShellLink.dll \
	UAC.dll \
	ServicesHelper.dll \
	$(NULL)

$(CONFIG_DIR)/setup.exe::
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/Plugins/,$(CUSTOM_NSIS_PLUGINS)) $(CONFIG_DIR)
	cd $(CONFIG_DIR) && $(MAKENSISU) installer.nsi
# Support for building the uninstaller when repackaging locales
ifeq ($(CONFIG_DIR),l10ngen)
	cd $(CONFIG_DIR) && $(MAKENSISU) uninstaller.nsi
endif
ifdef MOZ_EXTERNAL_SIGNING_FORMAT
	$(MOZ_SIGN_CMD) $(foreach f,$(MOZ_EXTERNAL_SIGNING_FORMAT),-f $(f)) "$@"
endif

$(CONFIG_DIR)/7zSD.sfx:
	$(CYGWIN_WRAPPER) upx --best -o $(CONFIG_DIR)/7zSD.sfx $(SFX_MODULE)

installer::
	$(INSTALL) $(CONFIG_DIR)/setup.exe $(DEPTH)/installer-stage
	cd $(DEPTH)/installer-stage && $(CYGWIN_WRAPPER) 7z a -r -t7z $(ABS_CONFIG_DIR)/app.7z -mx -m0=BCJ2 -m1=LZMA:d24 -m2=LZMA:d19 -m3=LZMA:d19  -mb0:1 -mb0s1:2 -mb0s2:3
	$(MAKE) $(CONFIG_DIR)/7zSD.sfx
	$(NSINSTALL) -D $(DIST)/$(PKG_INST_PATH)
	cat $(CONFIG_DIR)/7zSD.sfx $(CONFIG_DIR)/app.tag $(CONFIG_DIR)/app.7z > "$(DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe"
	chmod 0755 "$(DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe"
ifdef MOZ_EXTERNAL_SIGNING_FORMAT
	$(MOZ_SIGN_CMD) $(foreach f,$(MOZ_EXTERNAL_SIGNING_FORMAT),-f $(f)) "$(DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe"
endif

# For building the uninstaller during the application build so it can be
# included for mar file generation.
uninstaller::
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/Plugins/,$(CUSTOM_NSIS_PLUGINS)) $(CONFIG_DIR)
	cd $(CONFIG_DIR) && $(MAKENSISU) uninstaller.nsi
	$(NSINSTALL) -D $(DIST)/bin/uninstall
	cp $(CONFIG_DIR)/helper.exe $(DIST)/bin/uninstall

ifdef MOZ_MAINTENANCE_SERVICE
maintenanceservice_installer::
	cd $(CONFIG_DIR) && $(MAKENSISU) maintenanceservice_installer.nsi
	$(NSINSTALL) -D $(DIST)/bin/
	cp $(CONFIG_DIR)/maintenanceservice_installer.exe $(DIST)/bin
endif

ifdef MOZ_WEBAPP_RUNTIME
webapp_uninstaller::
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/Plugins/,$(CUSTOM_NSIS_PLUGINS)) $(CONFIG_DIR)
	cd $(CONFIG_DIR) && $(MAKENSISU) webapp-uninstaller.nsi
	$(NSINSTALL) -D $(FINAL_TARGET)
	cp $(CONFIG_DIR)/webapp-uninstaller.exe $(FINAL_TARGET)
endif
