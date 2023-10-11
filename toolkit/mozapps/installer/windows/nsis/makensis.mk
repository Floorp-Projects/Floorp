# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef CONFIG_DIR
$(error CONFIG_DIR must be set before including makensis.mk)
endif

ABS_CONFIG_DIR := $(abspath $(CONFIG_DIR))

SFX_MODULE ?= $(error SFX_MODULE is not defined)

ifeq ($(CPU_ARCH), aarch64)
USE_UPX := 
else
ifneq (,$(UPX)$(MOZ_AUTOMATION))
USE_UPX := --use-upx
endif
endif

TOOLKIT_NSIS_FILES = \
	common.nsh \
	locale.nlf \
	locale-fonts.nsh \
	locale-rtl.nlf \
	locales.nsi \
	overrides.nsh \
	setup.ico \
	$(NULL)

CUSTOM_NSIS_PLUGINS = \
	AccessControl.dll \
	AppAssocReg.dll \
	ApplicationID.dll \
	BitsUtils.dll \
	CertCheck.dll \
	CityHash.dll \
	ExecInExplorer.dll \
	HttpPostFile.dll \
	InetBgDL.dll \
	InvokeShellVerb.dll \
	liteFirewallW.dll \
	nsJSON.dll \
	PinToTaskbar.dll \
	ServicesHelper.dll \
	ShellLink.dll \
	UAC.dll \
	WebBrowser.dll \
	$(NULL)

CUSTOM_UI = \
	nsisui.exe \
	$(NULL)

$(CONFIG_DIR)/setup.exe::
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/Plugins/,$(CUSTOM_NSIS_PLUGINS)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/,$(CUSTOM_UI)) $(CONFIG_DIR)
	cd $(CONFIG_DIR) && $(MAKENSISU) $(MAKENSISU_FLAGS) installer.nsi
ifdef MOZ_STUB_INSTALLER
	cd $(CONFIG_DIR) && $(MAKENSISU) $(MAKENSISU_FLAGS) stub.nsi
endif

ifdef ZIP_IN
installer:: $(CONFIG_DIR)/setup.exe $(ZIP_IN)
	@echo 'Packaging $(WIN32_INSTALLER_OUT).'
	$(NSINSTALL) -D '$(ABS_DIST)/$(PKG_INST_PATH)'
	$(PYTHON3) $(MOZILLA_DIR)/mach repackage installer \
	  -o '$(ABS_DIST)/$(PKG_INST_PATH)$(PKG_INST_BASENAME).exe' \
	  --package-name '$(MOZ_PKG_DIR)' \
	  --package '$(ZIP_IN)' \
	  --tag $(topsrcdir)/$(MOZ_BUILD_APP)/installer/windows/app.tag \
	  --setupexe $(CONFIG_DIR)/setup.exe \
	  --sfx-stub $(SFX_MODULE) \
	  $(USE_UPX)
ifdef MOZ_STUB_INSTALLER
	$(PYTHON3) $(MOZILLA_DIR)/mach repackage installer \
	  -o '$(ABS_DIST)/$(PKG_INST_PATH)$(PKG_STUB_BASENAME).exe' \
	  --tag $(topsrcdir)/browser/installer/windows/stub.tag \
	  --setupexe $(CONFIG_DIR)/setup-stub.exe \
	  --sfx-stub $(SFX_MODULE) \
	  $(USE_UPX)
endif
else
installer::
	$(error ZIP_IN must be set when building installer)
endif

HELPER_DEPS = $(GLOBAL_DEPS) \
              $(addprefix $(srcdir)/,$(INSTALLER_FILES)) \
              $(addprefix $(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)/,$(BRANDING_FILES)) \
              $(srcdir)/nsis/defines.nsi.in \
              $(topsrcdir)/toolkit/mozapps/installer/windows/nsis/preprocess-locale.py \
              $(addprefix $(MOZILLA_DIR)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) \
              $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/Plugins/,$(CUSTOM_NSIS_PLUGINS))

# For building the uninstaller during the application build so it can be
# included for mar file generation.
$(CONFIG_DIR)/helper.exe: $(HELPER_DEPS)
	$(RM) -r $(CONFIG_DIR)
	$(MKDIR) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(srcdir)/,$(INSTALLER_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)/,$(BRANDING_FILES)) $(CONFIG_DIR)
	$(call py_action,preprocessor defines.nsi,-Fsubstitution $(DEFINES) $(ACDEFINES) \
	  $(srcdir)/nsis/defines.nsi.in -o $(CONFIG_DIR)/defines.nsi)
	$(PYTHON3) $(topsrcdir)/toolkit/mozapps/installer/windows/nsis/preprocess-locale.py \
	  --preprocess-locale $(topsrcdir) \
	  $(PPL_LOCALE_ARGS) $(AB_CD) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/Plugins/,$(CUSTOM_NSIS_PLUGINS)) $(CONFIG_DIR)
	cd $(CONFIG_DIR) && $(MAKENSISU) $(MAKENSISU_FLAGS) uninstaller.nsi

uninstaller:: $(CONFIG_DIR)/helper.exe
	$(NSINSTALL) -D $(DIST)/bin/uninstall
	cp $(CONFIG_DIR)/helper.exe $(DIST)/bin/uninstall

ifdef MOZ_MAINTENANCE_SERVICE
maintenanceservice_installer::
	$(RM) -r $(CONFIG_DIR)
	$(MKDIR) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(srcdir)/,$(INSTALLER_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(topsrcdir)/$(MOZ_BRANDING_DIRECTORY)/,$(BRANDING_FILES)) $(CONFIG_DIR)
	$(call py_action,preprocessor defines.nsi,-Fsubstitution $(DEFINES) $(ACDEFINES) \
	  $(srcdir)/nsis/defines.nsi.in -o $(CONFIG_DIR)/defines.nsi)
	$(PYTHON3) $(topsrcdir)/toolkit/mozapps/installer/windows/nsis/preprocess-locale.py \
	  --preprocess-locale $(topsrcdir) \
	  $(PPL_LOCALE_ARGS) $(AB_CD) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/toolkit/mozapps/installer/windows/nsis/,$(TOOLKIT_NSIS_FILES)) $(CONFIG_DIR)
	$(INSTALL) $(addprefix $(MOZILLA_DIR)/other-licenses/nsis/Plugins/,$(CUSTOM_NSIS_PLUGINS)) $(CONFIG_DIR)
	cd $(CONFIG_DIR) && $(MAKENSISU) $(MAKENSISU_FLAGS) maintenanceservice_installer.nsi
	$(NSINSTALL) -D $(DIST)/bin/
	cp $(CONFIG_DIR)/maintenanceservice_installer.exe $(DIST)/bin
endif
