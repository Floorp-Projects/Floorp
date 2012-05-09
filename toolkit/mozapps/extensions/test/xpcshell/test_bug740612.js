/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that attempts to override the global values fails but doesn't
// destroy the world with it
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

function getActiveVersion() {
  return Services.prefs.getIntPref("bootstraptest.active_version");
}

function getInstalledVersion() {
  return Services.prefs.getIntPref("bootstraptest.installed_version");
}

function manuallyInstall(aXPIFile, aInstallLocation, aID) {
  if (TEST_UNPACKED) {
    let dir = aInstallLocation.clone();
    dir.append(aID);
    dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
    let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
              createInstance(AM_Ci.nsIZipReader);
    zip.open(aXPIFile);
    let entries = zip.findEntries(null);
    while (entries.hasMore()) {
      let entry = entries.getNext();
      let target = dir.clone();
      entry.split("/").forEach(function(aPart) {
        target.append(aPart);
      });
      zip.extract(entry, target);
    }
    zip.close();

    return dir;
  }
  else {
    let target = aInstallLocation.clone();
    target.append(aID + ".xpi");
    aXPIFile.copyTo(target.parent, target.leafName);
    return target;
  }
}

function run_test() {
  do_test_pending();

  manuallyInstall(do_get_addon("test_bug740612_1"), profileDir,
                  "bug740612_1@tests.mozilla.org");
  manuallyInstall(do_get_addon("test_bug740612_2"), profileDir,
                  "bug740612_2@tests.mozilla.org");

  startupManager();

  AddonManager.getAddonsByIDs(["bug740612_1@tests.mozilla.org",
                               "bug740612_2@tests.mozilla.org"],
                               function([a1, a2]) {
    do_check_neq(a1, null);
    do_check_neq(a2, null);
    do_check_eq(getInstalledVersion(), "1.0");
    do_check_eq(getActiveVersion(), "1.0");

    do_test_finished();
  });
}
