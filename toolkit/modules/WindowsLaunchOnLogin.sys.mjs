/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
   * Checks if Windows launch on login was independently enabled or disabled
   * by the user in the Windows Startup Apps menu. The registry key that
   * stores this information should not be modified.
   */
  getLaunchOnLoginApproved() {
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
