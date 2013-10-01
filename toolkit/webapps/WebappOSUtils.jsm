/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const CC = Components.Constructor;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

this.EXPORTED_SYMBOLS = ["WebappOSUtils"];

// Returns the MD5 hash of a string.
function computeHash(aString) {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let result = {};
  // Data is an array of bytes.
  let data = converter.convertToByteArray(aString, result);

  let hasher = Cc["@mozilla.org/security/hash;1"].
               createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.MD5);
  hasher.update(data, data.length);
  // We're passing false to get the binary hash and not base64.
  let hash = hasher.finish(false);

  function toHexString(charCode) {
    return ("0" + charCode.toString(16)).slice(-2);
  }

  // Convert the binary hash data to a hex string.
  return [toHexString(hash.charCodeAt(i)) for (i in hash)].join("");
}

this.WebappOSUtils = {
  getUniqueName: function(aApp) {
    return this.sanitizeStringForFilename(aApp.name).toLowerCase() + "-" +
           computeHash(aApp.manifestURL);
  },

  /**
   * Returns the executable of the given app, identifying it by its unique name,
   * which is in either the new format or the old format.
   * On Mac OS X, it returns the identifier of the app.
   *
   * The new format ensures a readable and unique name for an app by combining
   * its name with a hash of its manifest URL.  The old format uses its origin,
   * which is only unique until we support multiple apps per origin.
   */
  getLaunchTarget: function(aApp) {
    let uniqueName = this.getUniqueName(aApp);

#ifdef XP_WIN
    let isOldNamingScheme = false;
    let appRegKey;
    try {
      let open = CC("@mozilla.org/windows-registry-key;1",
                    "nsIWindowsRegKey", "open");
      appRegKey = open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                       "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" +
                       uniqueName, Ci.nsIWindowsRegKey.ACCESS_READ);
    } catch (ex) {
      // Fall back to the old installation naming scheme
      try {
        appRegKey = open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                         "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" +
                         aApp.origin, Ci.nsIWindowsRegKey.ACCESS_READ);
        isOldNamingScheme = true;
      } catch (ex) {
        return null;
      }
    }

    let appFilename, installLocation;
    try {
      appFilename = appRegKey.readStringValue("AppFilename");
      installLocation = appRegKey.readStringValue("InstallLocation");
    } catch (ex) {
      return null;
    } finally {
      appRegKey.close();
    }

    installLocation = installLocation.substring(1, installLocation.length - 1);

    if (isOldNamingScheme &&
        !this.isOldInstallPathValid(aApp, installLocation)) {
      return null;
    }

    let initWithPath = CC("@mozilla.org/file/local;1",
                          "nsILocalFile", "initWithPath");
    let launchTarget = initWithPath(installLocation);
    launchTarget.append(appFilename + ".exe");

    return launchTarget;
#elifdef XP_MACOSX
    let mwaUtils = Cc["@mozilla.org/widget/mac-web-app-utils;1"]
                     .createInstance(Ci.nsIMacWebAppUtils);

    try {
      let path;
      if (path = mwaUtils.pathForAppWithIdentifier(uniqueName)) {
        return [ uniqueName, path ];
      }
    } catch(ex) {}

    // Fall back to the old installation naming scheme
    try {
      let path;
      if ((path = mwaUtils.pathForAppWithIdentifier(aApp.origin)) &&
           this.isOldInstallPathValid(aApp, path)) {
        return [ aApp.origin, path ];
      }
    } catch(ex) {}

    return [ null, null ];
#elifdef XP_UNIX
    let exeFile = Services.dirsvc.get("Home", Ci.nsIFile);
    exeFile.append("." + uniqueName);
    exeFile.append("webapprt-stub");

    // Fall back to the old installation naming scheme
    if (!exeFile.exists()) {
      exeFile = Services.dirsvc.get("Home", Ci.nsIFile);

      let origin = Services.io.newURI(aApp.origin, null, null);
      let installDir = "." + origin.scheme + ";" +
                       origin.host +
                       (origin.port != -1 ? ";" + origin.port : "");

      exeFile.append(installDir);
      exeFile.append("webapprt-stub");

      if (!exeFile.exists() ||
          !this.isOldInstallPathValid(aApp, exeFile.parent.path)) {
        return null;
      }
    }

    return exeFile;
#endif
  },

  getInstallPath: function(aApp) {
#ifdef MOZ_B2G
    // All b2g builds
    return aApp.basePath + "/" + aApp.id;

#elifdef MOZ_FENNEC
   // All fennec
    return aApp.basePath + "/" + aApp.id;

#elifdef MOZ_PHOENIX
   // Firefox

#ifdef XP_WIN
    let execFile = this.getLaunchTarget(aApp);
    if (!execFile) {
      return null;
    }

    return execFile.parent.path;
#elifdef XP_MACOSX
    let [ bundleID, path ] = this.getLaunchTarget(aApp);
    return path;
#elifdef XP_UNIX
    let execFile = this.getLaunchTarget(aApp);
    if (!execFile) {
      return null;
    }

    return execFile.parent.path;
#endif

#elifdef MOZ_WEBAPP_RUNTIME
    // Webapp runtime

#ifdef XP_WIN
    let execFile = this.getLaunchTarget(aApp);
    if (!execFile) {
      return null;
    }

    return execFile.parent.path;
#elifdef XP_MACOSX
    let [ bundleID, path ] = this.getLaunchTarget(aApp);
    return path;
#elifdef XP_UNIX
    let execFile = this.getLaunchTarget(aApp);
    if (!execFile) {
      return null;
    }

    return execFile.parent.path;
#endif

#endif
    // Anything unsupported, like Metro
    throw new Error("Unsupported apps platform");
  },

  launch: function(aApp) {
    let uniqueName = this.getUniqueName(aApp);

#ifdef XP_WIN
    let initProcess = CC("@mozilla.org/process/util;1",
                         "nsIProcess", "init");

    let launchTarget = this.getLaunchTarget(aApp);
    if (!launchTarget) {
      return false;
    }

    try {
      let process = initProcess(launchTarget);
      process.runwAsync([], 0);
    } catch (e) {
      return false;
    }

    return true;
#elifdef XP_MACOSX
    let [ launchIdentifier, path ] = this.getLaunchTarget(aApp);
    if (!launchIdentifier) {
      return false;
    }

    let mwaUtils = Cc["@mozilla.org/widget/mac-web-app-utils;1"]
                     .createInstance(Ci.nsIMacWebAppUtils);

    try {
      mwaUtils.launchAppWithIdentifier(launchIdentifier);
    } catch(e) {
      return false;
    }

    return true;
#elifdef XP_UNIX
    let exeFile = this.getLaunchTarget(aApp);
    if (!exeFile) {
      return false;
    }

    try {
      let process = Cc["@mozilla.org/process/util;1"]
                      .createInstance(Ci.nsIProcess);

      process.init(exeFile);
      process.runAsync([], 0);
    } catch (e) {
      return false;
    }

    return true;
#endif
  },

  uninstall: function(aApp) {
    let uniqueName = this.getUniqueName(aApp);

#ifdef XP_UNIX
#ifndef XP_MACOSX
    let exeFile = this.getLaunchTarget(aApp);
    if (!exeFile) {
      return false;
    }

    try {
      let process = Cc["@mozilla.org/process/util;1"]
                      .createInstance(Ci.nsIProcess);

      process.init(exeFile);
      process.runAsync(["-remove"], 1);
    } catch (e) {
      return false;
    }

    return true;
#endif
#endif
  },

  /**
   * Returns true if the given install path (in the old naming scheme) actually
   * belongs to the given application.
   */
  isOldInstallPathValid: function(aApp, aInstallPath) {
    // Applications with an origin that starts with "app" are packaged apps and
    // packaged apps have never been installed using the old naming scheme.
    // After bug 910465, we'll have a better way to check if an app is
    // packaged.
    if (aApp.origin.startsWith("app")) {
      return false;
    }

    // Bug 915480: We could check the app name from the manifest to
    // better verify the installation path.
    return true;
  },

  /**
   * Checks if the given app is locally installed.
   */
  isLaunchable: function(aApp) {
    let uniqueName = this.getUniqueName(aApp);

#ifdef XP_WIN
    if (!this.getLaunchTarget(aApp)) {
      return false;
    }

    return true;
#elifdef XP_MACOSX
    if (!this.getInstallPath(aApp)) {
      return false;
    }

    return true;
#elifdef XP_UNIX
    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);

    let xdg_data_home_env;
    try {
      xdg_data_home_env = env.get("XDG_DATA_HOME");
    } catch(ex) {}

    let desktopINI;
    if (xdg_data_home_env) {
      desktopINI = new FileUtils.File(xdg_data_home_env);
    } else {
      desktopINI = FileUtils.getFile("Home", [".local", "share"]);
    }
    desktopINI.append("applications");
    desktopINI.append("owa-" + uniqueName + ".desktop");

    // Fall back to the old installation naming scheme
    if (!desktopINI.exists()) {
      if (xdg_data_home_env) {
        desktopINI = new FileUtils.File(xdg_data_home_env);
      } else {
        desktopINI = FileUtils.getFile("Home", [".local", "share"]);
      }

      let origin = Services.io.newURI(aApp.origin, null, null);
      let oldUniqueName = origin.scheme + ";" +
                          origin.host +
                          (origin.port != -1 ? ";" + origin.port : "");

      desktopINI.append("owa-" + oldUniqueName + ".desktop");

      if (!desktopINI.exists()) {
        return false;
      }

      let installDir = Services.dirsvc.get("Home", Ci.nsIFile);
      installDir.append("." + origin.scheme + ";" + origin.host +
                        (origin.port != -1 ? ";" + origin.port : ""));

      return isOldInstallPathValid(aApp, installDir.path);
    }

    return true;
#endif
  },

  /**
   * Sanitize the filename (accepts only a-z, 0-9, - and _)
   */
  sanitizeStringForFilename: function(aPossiblyBadFilenameString) {
    return aPossiblyBadFilenameString.replace(/[^a-z0-9_\-]/gi, "");
  }
}
