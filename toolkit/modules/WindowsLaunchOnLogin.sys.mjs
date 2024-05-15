/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const LAUNCH_ON_LOGIN_TASKID = "LaunchOnLogin";

/**
 * "Launch on Login" is a Firefox feature automatically launches Firefox when the
 * user logs in to Windows. The technical mechanism is simply writing a registry
 * key to `Software\Microsoft\Windows\CurrentVersion\Run`, but there is an issue:
 * when disabled in the Windows UI, additional registry keys are written under
 * `Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run`. Any
 * keys stored here should be seen as a user override and should never be modified.
 * When such keys are present, the launch on login feature should be considered
 * disabled and not available from within Firefox. This module provides the
 * functionality to access and modify these registry keys.
 *
 * MSIX installs cannot write to the registry so we instead use the MSIX-exclusive
 * Windows StartupTask APIs. The difference here is that the startup task is always
 * "registered", we control whether it's enabled or disabled. As such some
 * functions such as getLaunchOnLoginApproved() have different behavior on MSIX installs.
 */
export var WindowsLaunchOnLogin = {
  /**
   * Accepts another function as an argument and provides an open Windows
   * launch on login registry key for the passed-in function to manipulate.
   *
   * @param func
   *        The function to use.
   */
  async withLaunchOnLoginRegistryKey(func) {
    let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );
    wrk.open(
      wrk.ROOT_KEY_CURRENT_USER,
      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
      wrk.ACCESS_ALL
    );
    try {
      await func(wrk);
    } finally {
      wrk.close();
    }
  },

  /**
   * Safely creates a Windows launch on login registry key
   */
  async createLaunchOnLoginRegistryKey() {
    try {
      await this.withLaunchOnLoginRegistryKey(async wrk => {
        // Added launch option -os-autostart for telemetry
        // Add quote marks around path to account for spaces in path
        let autostartPath =
          this.quoteString(Services.dirsvc.get("XREExeF", Ci.nsIFile).path) +
          " -os-autostart";
        try {
          wrk.writeStringValue(
            this.getLaunchOnLoginRegistryName(),
            autostartPath
          );
        } catch (e) {
          console.error("Could not write value to registry", e);
        }
      });
    } catch (e) {
      // We should only end up here if we fail to open the registry
      console.error("Failed to open Windows registry", e);
    }
  },

  /**
   * Either creates a Windows launch on login registry key on regular installs
   * or enables the startup task within the app manifest due to
   * restrictions on writing to the registry in MSIX.
   */
  async createLaunchOnLogin() {
    if (Services.sysinfo.getProperty("hasWinPackageId")) {
      await this.enableLaunchOnLoginMSIX();
    } else {
      await this.createLaunchOnLoginRegistryKey();
    }
  },

  /**
   * Either deletes a Windows launch on login registry key and shortcut on
   * regular installs or disables the startup task within the app manifest due
   * to restrictions on writing to the registry in MSIX.
   */
  async removeLaunchOnLogin() {
    if (Services.sysinfo.getProperty("hasWinPackageId")) {
      await this.disableLaunchOnLoginMSIX();
    } else {
      await this.removeLaunchOnLoginRegistryKey();
      await this.removeLaunchOnLoginShortcuts();
    }
  },

  /**
   * Safely removes a Windows launch on login registry key
   */
  async removeLaunchOnLoginRegistryKey() {
    try {
      await this.withLaunchOnLoginRegistryKey(async wrk => {
        let registryName = this.getLaunchOnLoginRegistryName();
        if (wrk.hasValue(registryName)) {
          try {
            wrk.removeValue(registryName);
          } catch (e) {
            console.error("Failed to remove Windows registry value", e);
          }
        }
      });
    } catch (e) {
      // We should only end up here if we fail to open the registry
      console.error("Failed to open Windows registry", e);
    }
  },

  /**
   * Enables launch on login on MSIX installs by using the
   * StartupTask APIs. A task called "LaunchOnLogin" exists
   * in the packaged application manifest.
   *
   * @returns {Promise<bool>}
   *          Whether the enable operation was successful.
   */
  async enableLaunchOnLoginMSIX() {
    if (!Services.sysinfo.getProperty("hasWinPackageId")) {
      throw Components.Exception(
        "Called on non-MSIX build",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }
    let shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
      Ci.nsIWindowsShellService
    );
    return shellService.enableLaunchOnLoginMSIXAsync(LAUNCH_ON_LOGIN_TASKID);
  },

  /**
   * Disables launch on login on MSIX installs by using the
   * StartupTask APIs. A task called "LaunchOnLogin" exists
   * in the packaged application manifest.
   *
   * @returns {Promise<bool>}
   *          Whether the disable operation was successful.
   */
  async disableLaunchOnLoginMSIX() {
    if (!Services.sysinfo.getProperty("hasWinPackageId")) {
      throw Components.Exception(
        "Called on non-MSIX build",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }
    let shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
      Ci.nsIWindowsShellService
    );
    return shellService.disableLaunchOnLoginMSIXAsync(LAUNCH_ON_LOGIN_TASKID);
  },

  /**
   * Determines whether launch on login on MSIX is enabled by using the
   * StartupTask APIs. A task called "LaunchOnLogin" exists
   * in the packaged application manifest.
   *
   * @returns {Promise<bool>}
   *          Whether the startup task is enabled.
   */
  async getLaunchOnLoginEnabledMSIX() {
    if (!Services.sysinfo.getProperty("hasWinPackageId")) {
      throw Components.Exception(
        "Called on non-MSIX build",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }
    let shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
      Ci.nsIWindowsShellService
    );
    let state = await shellService.getLaunchOnLoginEnabledMSIXAsync(
      LAUNCH_ON_LOGIN_TASKID
    );
    return (
      state == shellService.LAUNCH_ON_LOGIN_ENABLED ||
      state == shellService.LAUNCH_ON_LOGIN_ENABLED_BY_POLICY
    );
  },

  /**
   * Gets a list of all launch on login shortcuts in the
   * %USERNAME%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup folder
   * that point to the current Firefox executable.
   */
  getLaunchOnLoginShortcutList() {
    let shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
      Ci.nsIWindowsShellService
    );
    return shellService.getLaunchOnLoginShortcuts();
  },

  /**
   * Safely removes all launch on login shortcuts in the
   * %USERNAME%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup folder
   * that point to the current Firefox executable.
   */
  async removeLaunchOnLoginShortcuts() {
    let shortcuts = this.getLaunchOnLoginShortcutList();
    for (let i = 0; i < shortcuts.length; i++) {
      await IOUtils.remove(shortcuts[i]);
    }
  },

  /**
   * If the state is set to disabled from the Windows UI our API calls to
   * re-enable it will fail so we should say that it's not approved and
   * provide users the link to the App Startup settings so they can
   * re-enable it.
   *
   * @returns {Promise<bool>}
   *          If launch on login has not been disabled by Windows settings
   *          or enabled by policy.
   */
  async getLaunchOnLoginApprovedMSIX() {
    if (!Services.sysinfo.getProperty("hasWinPackageId")) {
      throw Components.Exception(
        "Called on non-MSIX build",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }
    let shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
      Ci.nsIWindowsShellService
    );
    let state = await shellService.getLaunchOnLoginEnabledMSIXAsync(
      LAUNCH_ON_LOGIN_TASKID
    );
    return !(
      state == shellService.LAUNCH_ON_LOGIN_DISABLED_BY_SETTINGS ||
      state == shellService.LAUNCH_ON_LOGIN_ENABLED_BY_POLICY
    );
  },

  /**
   * Checks if Windows launch on login was independently enabled or disabled
   * by the user in the Windows Startup Apps menu. The registry key that
   * stores this information should not be modified.
   *
   * If the state is set to disabled from the Windows UI on MSIX our API calls to
   * re-enable it will fail so report false.
   *
   * @returns {Promise<bool>}
   *          Report whether launch on login is allowed on Windows. On MSIX
   *          it's possible to set a startup app through policy making us
   *          unable to modify it so we should account for that here.
   */
  async getLaunchOnLoginApproved() {
    if (Services.sysinfo.getProperty("hasWinPackageId")) {
      return this.getLaunchOnLoginApprovedMSIX();
    }
    try {
      let wrkApproved = Cc[
        "@mozilla.org/windows-registry-key;1"
      ].createInstance(Ci.nsIWindowsRegKey);
      wrkApproved.open(
        wrkApproved.ROOT_KEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
        wrkApproved.ACCESS_READ
      );
      let registryName = this.getLaunchOnLoginRegistryName();
      if (!wrkApproved.hasValue(registryName)) {
        wrkApproved.close();
        return true;
      }
      // There's very little consistency with these binary values aside from if the first byte
      // is even it's enabled and odd is disabled. There's also no published specification.
      let approvedByWindows =
        wrkApproved.readBinaryValue(registryName).charCodeAt(0).toString(16) %
          2 ==
        0;
      wrkApproved.close();
      return approvedByWindows;
    } catch (e) {
      // We should only end up here if we fail to open the registry
      console.error("Failed to open Windows registry", e);
    }
    return true;
  },

  /**
   * Checks if Windows launch on login has an existing registry key or user-created shortcut in
   * %USERNAME%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup. The registry key that
   * stores this information should not be modified.
   *
   * On MSIX installs we instead query whether the StartupTask is enabled or disabled
   *
   * @returns {Promise<bool>}
   *          Whether launch on login is enabled.
   */
  async getLaunchOnLoginEnabled() {
    if (Services.sysinfo.getProperty("hasWinPackageId")) {
      return this.getLaunchOnLoginEnabledMSIX();
    }

    let registryName = this.getLaunchOnLoginRegistryName();
    let regExists = false;
    let shortcutExists = false;
    this.withLaunchOnLoginRegistryKey(wrk => {
      try {
        // Start by checking if registry key exists.
        regExists = wrk.hasValue(registryName);
      } catch (e) {
        // We should only end up here if we fail to open the registry
        console.error("Failed to open Windows registry", e);
      }
    });
    if (!regExists) {
      shortcutExists = !!this.getLaunchOnLoginShortcutList().length;
    }
    // Even if a user disables it later on we want the launch on login
    // infobar to remain disabled as the user is aware of the option.
    if (regExists || shortcutExists) {
      Services.prefs.setBoolPref(
        "browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt",
        true
      );
    }
    return regExists || shortcutExists;
  },

  /**
   * Quotes a string for use as a single command argument, using Windows quoting
   * conventions.
   *
   * @see https://msdn.microsoft.com/en-us/library/17w5ykft(v=vs.85).aspx
   *
   * @param {string} str
   *        The argument string to quote.
   * @returns {string}
   */
  quoteString(str) {
    if (!/[\s"]/.test(str)) {
      return str;
    }

    let escaped = str.replace(/(\\*)("|$)/g, (m0, m1, m2) => {
      if (m2) {
        m2 = `\\${m2}`;
      }
      return `${m1}${m1}${m2}`;
    });

    return `"${escaped}"`;
  },

  /**
   * Generates a unique registry name for the current application
   * like "Mozilla-Firefox-71AE18FE3142402B".
   */
  getLaunchOnLoginRegistryName() {
    let xreDirProvider = Cc[
      "@mozilla.org/xre/directory-provider;1"
    ].createInstance(Ci.nsIXREDirProvider);
    let registryName = `${Services.appinfo.vendor}-${
      Services.appinfo.name
    }-${xreDirProvider.getInstallHash()}`;
    return registryName;
  },
};
