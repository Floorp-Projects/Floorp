/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const CC = Components.Constructor;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

let EXPORTED_SYMBOLS = ["WebappOSUtils"];

let WebappOSUtils = {
  launch: function(aData) {
#ifdef XP_WIN
    let appRegKey;
    try {
      let open = CC("@mozilla.org/windows-registry-key;1",
                    "nsIWindowsRegKey", "open");
      let initWithPath = CC("@mozilla.org/file/local;1",
                            "nsILocalFile", "initWithPath");
      let initProcess = CC("@mozilla.org/process/util;1",
                           "nsIProcess", "init");

      appRegKey = open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                       "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" +
                       aData.origin, Ci.nsIWindowsRegKey.ACCESS_READ);

      let launchTarget = initWithPath(appRegKey.readStringValue("InstallLocation"));
      launchTarget.append(appRegKey.readStringValue("AppFilename") + ".exe");

      let process = initProcess(launchTarget);
      process.runwAsync([], 0);
    } catch (e) {
      return false;
    } finally {
      if (appRegKey) {
        appRegKey.close();
      }
    }

    return true;
#elifdef XP_MACOSX
    let mwaUtils = Cc["@mozilla.org/widget/mac-web-app-utils;1"]
                    .createInstance(Ci.nsIMacWebAppUtils);
    let appPath;
    try {
      appPath = mwaUtils.pathForAppWithIdentifier(aData.origin);
    } catch (e) {}

    if (appPath) {
      mwaUtils.launchAppWithIdentifier(aData.origin);
      return true;
    }

    return false;
#elifdef XP_UNIX
    let origin = Services.io.newURI(aData.origin, null, null);
    let installDir = "." + origin.scheme + ";" + origin.host;
    if (origin.port != -1)
      installDir += ";" + origin.port;

    let exeFile = Services.dirsvc.get("Home", Ci.nsIFile);
    exeFile.append(installDir);
    exeFile.append("webapprt-stub");

    try {
      if (exeFile.exists()) {
        let process = Cc["@mozilla.org/process/util;1"]
                        .createInstance(Ci.nsIProcess);
        process.init(exeFile);
        process.runAsync([], 0);
        return true;
      }
    } catch (e) {}

    return false;
#endif
  }
}
