/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that moving an extension in the filesystem without any other
// change still keeps updated compatibility information

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
// Enable loading extensions from the user and system scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "1.9.2");

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));

var userDir = gProfD.clone();
userDir.append("extensions2");
userDir.append(gAppInfo.ID);

var dirProvider = {
  getFile(aProp, aPersistent) {
    aPersistent.value = false;
    if (aProp == "XREUSysExt")
      return userDir.parent;
    return null;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIDirectoryServiceProvider])
};
Services.dirsvc.registerProvider(dirProvider);

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  bootstrap: true,
  updateURL: "http://example.com/data/test_bug655254.json",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const ADDONS = [
  {
    "install.rdf": {
      id: "addon2@tests.mozilla.org",
      version: "1.0",
      name: "Test 2",
      bootstrap: true,

      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "2",
        maxVersion: "2"}],
    },
    "bootstrap.js": `
      /* exported startup, shutdown */
      ChromeUtils.import("resource://gre/modules/Services.jsm");

      function startup(data, reason) {
        Services.prefs.setIntPref("bootstraptest.active_version", 1);
      }

      function shutdown(data, reason) {
        Services.prefs.setIntPref("bootstraptest.active_version", 0);
      }
    `
  },
];

const XPIS = ADDONS.map(addon => AddonTestUtils.createTempXPIFile(addon));

add_task(async function test_1() {
  var time = Date.now();
  var dir = await promiseWriteInstallRDFForExtension(addon1, userDir);
  setExtensionModifiedTime(dir, time);

  await manuallyInstall(XPIS[0], userDir, "addon2@tests.mozilla.org");

  await promiseStartupManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                                    "addon2@tests.mozilla.org"]);
  Assert.notEqual(a1, null);
  Assert.ok(a1.appDisabled);
  Assert.ok(!a1.isActive);
  Assert.ok(!isExtensionInBootstrappedList(userDir, a1.id));

  Assert.notEqual(a2, null);
  Assert.ok(!a2.appDisabled);
  Assert.ok(a2.isActive);
  Assert.ok(isExtensionInBootstrappedList(userDir, a2.id));
  Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

  await AddonTestUtils.promiseFindAddonUpdates(a1, AddonManager.UPDATE_WHEN_USER_REQUESTED);

  await promiseRestartManager();

  let a1_2 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1_2, null);
  Assert.ok(!a1_2.appDisabled);
  Assert.ok(a1_2.isActive);
  Assert.ok(isExtensionInBootstrappedList(userDir, a1_2.id));

  await promiseShutdownManager();

  Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

  userDir.parent.moveTo(gProfD, "extensions3");
  userDir = gProfD.clone();
  userDir.append("extensions3");
  userDir.append(gAppInfo.ID);
  Assert.ok(userDir.exists());

  await promiseStartupManager();

  let [a1_3, a2_3] = await AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                                        "addon2@tests.mozilla.org"]);
  Assert.notEqual(a1_3, null);
  Assert.ok(!a1_3.appDisabled);
  Assert.ok(a1_3.isActive);
  Assert.ok(isExtensionInBootstrappedList(userDir, a1_3.id));

  Assert.notEqual(a2_3, null);
  Assert.ok(!a2_3.appDisabled);
  Assert.ok(a2_3.isActive);
  Assert.ok(isExtensionInBootstrappedList(userDir, a2_3.id));
  Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 1);
});

// Set up the profile
add_task(async function test_2() {
  let a2 = await AddonManager.getAddonByID("addon2@tests.mozilla.org");
 Assert.notEqual(a2, null);
 Assert.ok(!a2.appDisabled);
 Assert.ok(a2.isActive);
 Assert.ok(isExtensionInBootstrappedList(userDir, a2.id));
 Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

 a2.userDisabled = true;
 Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

 await promiseShutdownManager();

 userDir.parent.moveTo(gProfD, "extensions4");
 userDir = gProfD.clone();
 userDir.append("extensions4");
 userDir.append(gAppInfo.ID);
 Assert.ok(userDir.exists());

 await promiseStartupManager();

 let [a1_2, a2_2] = await AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                              "addon2@tests.mozilla.org"]);
 Assert.notEqual(a1_2, null);
 Assert.ok(!a1_2.appDisabled);
 Assert.ok(a1_2.isActive);
 Assert.ok(isExtensionInBootstrappedList(userDir, a1_2.id));

 Assert.notEqual(a2_2, null);
 Assert.ok(a2_2.userDisabled);
 Assert.ok(!a2_2.isActive);
 Assert.ok(!isExtensionInBootstrappedList(userDir, a2_2.id));
 Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 0);
});
