/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we migrate data from the old extensions.rdf database. This
// matches test_migrate1.js however it runs with a lightweight theme selected
// so the themes should appear disabled.

Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "2.0",
  name: "Test 2",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "2.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "2.0",
  name: "Test 4",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "2.0",
  name: "Test 5",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var theme1 = {
  id: "theme1@tests.mozilla.org",
  version: "1.0",
  name: "Theme 1",
  type: 4,
  internalName: "theme1/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

var theme2 = {
  id: "theme2@tests.mozilla.org",
  version: "1.0",
  name: "Theme 2",
  type: 4,
  internalName: "theme2/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(theme1, profileDir);
  writeInstallRDFForExtension(theme2, profileDir);

  // Cannot use the LightweightThemeManager before AddonManager has been started
  // so inject the correct prefs
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify([{
    id: "1",
    version: "1",
    name: "Test LW Theme",
    description: "A test theme",
    author: "Mozilla",
    homepageURL: "http://localhost:4444/data/index.html",
    headerURL: "http://localhost:4444/data/header.png",
    footerURL: "http://localhost:4444/data/footer.png",
    previewURL: "http://localhost:4444/data/preview.png",
    iconURL: "http://localhost:4444/data/icon.png"
  }]));
  Services.prefs.setBoolPref("lightweightThemes.isThemeSelected", true);

  let stagedXPIs = profileDir.clone();
  stagedXPIs.append("staged-xpis");
  stagedXPIs.append("addon6@tests.mozilla.org");
  stagedXPIs.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);

  let addon6 = do_get_addon("test_migrate6");
  addon6.copyTo(stagedXPIs, "tmp.xpi");
  stagedXPIs = stagedXPIs.parent;

  stagedXPIs.append("addon7@tests.mozilla.org");
  stagedXPIs.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);

  let addon7 = do_get_addon("test_migrate7");
  addon7.copyTo(stagedXPIs, "tmp.xpi");
  stagedXPIs = stagedXPIs.parent;

  let old = do_get_file("data/test_migrate.rdf");
  old.copyTo(gProfD, "extensions.rdf");

  // Theme state is determined by the selected theme pref
  Services.prefs.setCharPref("general.skins.selectedSkin", "theme1/1.0");

  startupManager();
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([a1, a2, a3,
                                                                      a4, a5, a6,
                                                                      a7, t1, t2]) {
    // addon1 was user and app enabled in the old extensions.rdf
    do_check_neq(a1, null);
    do_check_false(a1.userDisabled);
    do_check_false(a1.appDisabled);
    do_check_true(a1.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, a1.id));

    // addon2 was user disabled and app enabled in the old extensions.rdf
    do_check_neq(a2, null);
    do_check_true(a2.userDisabled);
    do_check_false(a2.appDisabled);
    do_check_false(a2.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, a2.id));

    // addon3 was pending user disable and app disabled in the old extensions.rdf
    do_check_neq(a3, null);
    do_check_true(a3.userDisabled);
    do_check_true(a3.appDisabled);
    do_check_false(a3.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, a3.id));

    // addon4 was pending user enable and app disabled in the old extensions.rdf
    do_check_neq(a4, null);
    do_check_false(a4.userDisabled);
    do_check_true(a4.appDisabled);
    do_check_false(a4.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, a4.id));

    // addon5 was disabled and compatible but a new version has been installed
    // since, it should still be disabled but should be incompatible
    do_check_neq(a5, null);
    do_check_true(a5.userDisabled);
    do_check_true(a5.appDisabled);
    do_check_false(a5.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, a5.id));

    // addon6 should be installed and compatible and packed unless unpacking is
    // forced
    do_check_neq(a6, null);
    do_check_false(a6.userDisabled);
    do_check_false(a6.appDisabled);
    do_check_true(a6.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, a6.id));
    if (Services.prefs.getBoolPref("extensions.alwaysUnpack"))
      do_check_eq(a6.getResourceURI("install.rdf").scheme, "file");
    else
      do_check_eq(a6.getResourceURI("install.rdf").scheme, "jar");

    // addon7 should be installed and compatible and unpacked
    do_check_neq(a7, null);
    do_check_false(a7.userDisabled);
    do_check_false(a7.appDisabled);
    do_check_true(a7.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, a7.id));
    do_check_eq(a7.getResourceURI("install.rdf").scheme, "file");

    // Theme 1 was previously disabled
    do_check_neq(t1, null);
    do_check_true(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_false(t1.isActive);
    do_check_true(isThemeInAddonsList(profileDir, t1.id));
    do_check_true(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    // Theme 2 was previously disabled
    do_check_neq(t1, null);
    do_check_true(t2.userDisabled);
    do_check_false(t2.appDisabled);
    do_check_false(t2.isActive);
    do_check_false(isThemeInAddonsList(profileDir, t2.id));
    do_check_true(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_false(stagedXPIs.exists());

    do_test_finished();
  });
}
