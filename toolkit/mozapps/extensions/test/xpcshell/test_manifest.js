/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests that all properties are read from the install manifests and that
// items are correctly enabled/disabled based on them (blocklist tests are
// elsewhere)

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  const profileDir = gProfD.clone();
  profileDir.append("extensions");

  writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    aboutURL: "chrome://test/content/about.xul",
    iconURL: "chrome://test/skin/icon.png",
    icon64URL: "chrome://test/skin/icon64.png",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
    description: "Test Description",
    creator: "Test Creator",
    homepageURL: "http://www.example.com",
    developer: [
      "Test Developer 1",
      "Test Developer 2"
    ],
    translator: [
      "Test Translator 1",
      "Test Translator 2"
    ],
    contributor: [
      "Test Contributor 1",
      "Test Contributor 2"
    ]
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon2@tests.mozilla.org",
    version: "1.0",
    updateURL: "https://www.foo.com",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 2"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon3@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://www.foo.com",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 3"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon4@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://www.foo.com",
    updateKey: "foo",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 4"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon5@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "*"
    }],
    name: "Test Addon 5"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon6@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "0",
      maxVersion: "1"
    }],
    name: "Test Addon 6"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon7@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "0",
      maxVersion: "0"
    }],
    name: "Test Addon 7"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon8@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1.1",
      maxVersion: "*"
    }],
    name: "Test Addon 8"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon9@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "toolkit@mozilla.org",
      minVersion: "1.9.2",
      maxVersion: "1.9.*"
    }],
    name: "Test Addon 9"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon10@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "toolkit@mozilla.org",
      minVersion: "1.9.2.1",
      maxVersion: "1.9.*"
    }],
    name: "Test Addon 10"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon11@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "toolkit@mozilla.org",
      minVersion: "1.9",
      maxVersion: "1.9.2"
    }],
    name: "Test Addon 11"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon12@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "toolkit@mozilla.org",
      minVersion: "1.9",
      maxVersion: "1.9.1.*"
    }],
    name: "Test Addon 12"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon13@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "toolkit@mozilla.org",
      minVersion: "1.9",
      maxVersion: "1.9.*"
    }, {
      id: "xpcshell@tests.mozilla.org",
      minVersion: "0",
      maxVersion: "0.5"
    }],
    name: "Test Addon 13"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon14@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "toolkit@mozilla.org",
      minVersion: "1.9",
      maxVersion: "1.9.1"
    }, {
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 14"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon15@tests.mozilla.org",
    version: "1.0",
    updateKey: "foo",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 15"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon16@tests.mozilla.org",
    version: "1.0",
    updateKey: "foo",
    updateURL: "https://www.foo.com",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 16"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon17@tests.mozilla.org",
    version: "1.0",
    optionsURL: "chrome://test/content/options.xul",
    optionsType: "2",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 17"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon18@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 18"
  }, profileDir, undefined, "options.xul");

  writeInstallRDFForExtension({
    id: "addon19@tests.mozilla.org",
    version: "1.0",
    optionsType: "99",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 19"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon20@tests.mozilla.org",
    version: "1.0",
    optionsURL: "chrome://test/content/options.xul",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 20"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon21@tests.mozilla.org",
    version: "1.0",
    optionsType: "3",
    optionsURL: "chrome://test/content/options.xul",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 21"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon22@tests.mozilla.org",
    version: "1.0",
    optionsType: "2",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 22"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon23@tests.mozilla.org",
    version: "1.0",
    optionsType: "2",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 23"
  }, profileDir, undefined, "options.xul");

  writeInstallRDFForExtension({
    id: "addon24@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 24"
  }, profileDir, undefined, "options.xul");

  writeInstallRDFForExtension({
    id: "addon25@tests.mozilla.org",
    version: "1.0",
    optionsType: "3",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 25"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "addon26@tests.mozilla.org",
    version: "1.0",
    optionsType: "4",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 26"
  }, profileDir, undefined, "options.xul");

  do_test_pending();
  startupManager();
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "addon8@tests.mozilla.org",
                               "addon9@tests.mozilla.org",
                               "addon10@tests.mozilla.org",
                               "addon11@tests.mozilla.org",
                               "addon12@tests.mozilla.org",
                               "addon13@tests.mozilla.org",
                               "addon14@tests.mozilla.org",
                               "addon15@tests.mozilla.org",
                               "addon16@tests.mozilla.org",
                               "addon17@tests.mozilla.org",
                               "addon18@tests.mozilla.org",
                               "addon19@tests.mozilla.org",
                               "addon20@tests.mozilla.org",
                               "addon21@tests.mozilla.org",
                               "addon22@tests.mozilla.org",
                               "addon23@tests.mozilla.org",
                               "addon24@tests.mozilla.org",
                               "addon25@tests.mozilla.org",
                               "addon26@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5, a6, a7, a8, a9, a10,
                                         a11, a12, a13, a14, a15, a16, a17, a18, a19, a20,
                                         a21, a22, a23, a24, a25, a26]) {

    Assert.notEqual(a1, null);
    Assert.equal(a1.id, "addon1@tests.mozilla.org");
    Assert.equal(a1.type, "extension");
    Assert.equal(a1.version, "1.0");
    Assert.equal(a1.optionsType, null);
    Assert.equal(a1.aboutURL, "chrome://test/content/about.xul");
    Assert.equal(a1.iconURL, "chrome://test/skin/icon.png");
    Assert.equal(a1.icon64URL, "chrome://test/skin/icon64.png");
    Assert.equal(a1.icons[32], "chrome://test/skin/icon.png");
    Assert.equal(a1.icons[64], "chrome://test/skin/icon64.png");
    Assert.equal(a1.name, "Test Addon 1");
    Assert.equal(a1.description, "Test Description");
    Assert.equal(a1.creator, "Test Creator");
    Assert.equal(a1.homepageURL, "http://www.example.com");
    Assert.equal(a1.developers[0], "Test Developer 1");
    Assert.equal(a1.developers[1], "Test Developer 2");
    Assert.equal(a1.translators[0], "Test Translator 1");
    Assert.equal(a1.translators[1], "Test Translator 2");
    Assert.equal(a1.contributors[0], "Test Contributor 1");
    Assert.equal(a1.contributors[1], "Test Contributor 2");
    Assert.ok(a1.isActive);
    Assert.ok(!a1.userDisabled);
    Assert.ok(!a1.appDisabled);
    Assert.ok(a1.isCompatible);
    Assert.ok(a1.providesUpdatesSecurely);
    Assert.equal(a1.blocklistState, AM_Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

    Assert.notEqual(a2, null);
    Assert.equal(a2.id, "addon2@tests.mozilla.org");
    Assert.ok(a2.isActive);
    Assert.ok(!a2.userDisabled);
    Assert.ok(!a2.appDisabled);
    Assert.ok(a2.providesUpdatesSecurely);

    Assert.notEqual(a3, null);
    Assert.equal(a3.id, "addon3@tests.mozilla.org");
    Assert.ok(!a3.isActive);
    Assert.ok(!a3.userDisabled);
    Assert.ok(a3.appDisabled);
    Assert.ok(!a3.providesUpdatesSecurely);

    Assert.notEqual(a4, null);
    Assert.equal(a4.id, "addon4@tests.mozilla.org");
    Assert.ok(a4.isActive);
    Assert.ok(!a4.userDisabled);
    Assert.ok(!a4.appDisabled);
    Assert.ok(a4.providesUpdatesSecurely);

    Assert.notEqual(a5, null);
    Assert.ok(a5.isActive);
    Assert.ok(!a5.userDisabled);
    Assert.ok(!a5.appDisabled);
    Assert.ok(a5.isCompatible);

    Assert.notEqual(a6, null);
    Assert.ok(a6.isActive);
    Assert.ok(!a6.userDisabled);
    Assert.ok(!a6.appDisabled);
    Assert.ok(a6.isCompatible);

    Assert.notEqual(a7, null);
    Assert.ok(!a7.isActive);
    Assert.ok(!a7.userDisabled);
    Assert.ok(a7.appDisabled);
    Assert.ok(!a7.isCompatible);

    Assert.notEqual(a8, null);
    Assert.ok(!a8.isActive);
    Assert.ok(!a8.userDisabled);
    Assert.ok(a8.appDisabled);
    Assert.ok(!a8.isCompatible);

    Assert.notEqual(a9, null);
    Assert.ok(a9.isActive);
    Assert.ok(!a9.userDisabled);
    Assert.ok(!a9.appDisabled);
    Assert.ok(a9.isCompatible);

    Assert.notEqual(a10, null);
    Assert.ok(!a10.isActive);
    Assert.ok(!a10.userDisabled);
    Assert.ok(a10.appDisabled);
    Assert.ok(!a10.isCompatible);

    Assert.notEqual(a11, null);
    Assert.ok(a11.isActive);
    Assert.ok(!a11.userDisabled);
    Assert.ok(!a11.appDisabled);
    Assert.ok(a11.isCompatible);

    Assert.notEqual(a12, null);
    Assert.ok(!a12.isActive);
    Assert.ok(!a12.userDisabled);
    Assert.ok(a12.appDisabled);
    Assert.ok(!a12.isCompatible);

    Assert.notEqual(a13, null);
    Assert.ok(!a13.isActive);
    Assert.ok(!a13.userDisabled);
    Assert.ok(a13.appDisabled);
    Assert.ok(!a13.isCompatible);

    Assert.notEqual(a14, null);
    Assert.ok(a14.isActive);
    Assert.ok(!a14.userDisabled);
    Assert.ok(!a14.appDisabled);
    Assert.ok(a14.isCompatible);

    Assert.notEqual(a15, null);
    Assert.ok(a15.isActive);
    Assert.ok(!a15.userDisabled);
    Assert.ok(!a15.appDisabled);
    Assert.ok(a15.isCompatible);
    Assert.ok(a15.providesUpdatesSecurely);

    Assert.notEqual(a16, null);
    Assert.ok(a16.isActive);
    Assert.ok(!a16.userDisabled);
    Assert.ok(!a16.appDisabled);
    Assert.ok(a16.isCompatible);
    Assert.ok(a16.providesUpdatesSecurely);

    // An obsolete optionsType means the add-on isn't registered.
    Assert.equal(a17, null);

    Assert.notEqual(a18, null);
    Assert.ok(a18.isActive);
    Assert.ok(!a18.userDisabled);
    Assert.ok(!a18.appDisabled);
    Assert.ok(a18.isCompatible);
    Assert.equal(a18.optionsURL, null);
    Assert.equal(a18.optionsType, null);

    Assert.equal(a19, null);

    // Even with a defined optionsURL optionsType is null by default.
    Assert.notEqual(a20, null);
    Assert.ok(a20.isActive);
    Assert.ok(!a20.userDisabled);
    Assert.ok(!a20.appDisabled);
    Assert.ok(a20.isCompatible);
    Assert.equal(a20.optionsURL, "chrome://test/content/options.xul");
    Assert.equal(a20.optionsType, null);

    Assert.notEqual(a21, null);
    Assert.ok(a21.isActive);
    Assert.ok(!a21.userDisabled);
    Assert.ok(!a21.appDisabled);
    Assert.ok(a21.isCompatible);
    Assert.equal(a21.optionsURL, "chrome://test/content/options.xul");
    Assert.equal(a21.optionsType, AddonManager.OPTIONS_TYPE_TAB);

    // An obsolete optionsType means the add-on isn't registered.
    Assert.equal(a22, null);

    // An obsolete optionsType means the add-on isn't registered.
    Assert.equal(a23, null);

    Assert.notEqual(a24, null);
    Assert.equal(a24.optionsType, null);
    Assert.equal(a24.optionsURL, null);

    Assert.notEqual(a25, null);
    Assert.equal(a25.optionsType, null);
    Assert.equal(a25.optionsURL, null);

    // An obsolete optionsType means the add-on isn't registered.
    Assert.equal(a26, null);

    executeSoon(do_test_finished);
  });
}
