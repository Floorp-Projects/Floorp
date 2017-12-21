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

Components.utils.import("resource://testing-common/httpd.js");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;
mapFile("/data/test_bug655254.rdf", testserver);

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

  QueryInterface: XPCOMUtils.generateQI([AM_Ci.nsIDirectoryServiceProvider,
                                         AM_Ci.nsISupports])
};
Services.dirsvc.registerProvider(dirProvider);

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  updateURL: "http://localhost:" + gPort + "/data/test_bug655254.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Set up the profile
function run_test() {
  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

async function run_test_1() {
  var time = Date.now();
  var dir = writeInstallRDFForExtension(addon1, userDir);
  setExtensionModifiedTime(dir, time);

  manuallyInstall(do_get_addon("test_bug655254_2"), userDir, "addon2@tests.mozilla.org");

  await promiseStartupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a1, a2]) {
    Assert.notEqual(a1, null);
    Assert.ok(a1.appDisabled);
    Assert.ok(!a1.isActive);
    Assert.ok(!isExtensionInAddonsList(userDir, a1.id));

    Assert.notEqual(a2, null);
    Assert.ok(!a2.appDisabled);
    Assert.ok(a2.isActive);
    Assert.ok(!isExtensionInAddonsList(userDir, a2.id));
    Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

    a1.findUpdates({
      async onUpdateFinished() {
        await promiseRestartManager();

        AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(a1_2) {
          Assert.notEqual(a1_2, null);
          Assert.ok(!a1_2.appDisabled);
          Assert.ok(a1_2.isActive);
          Assert.ok(isExtensionInAddonsList(userDir, a1_2.id));

          shutdownManager();

          Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

          userDir.parent.moveTo(gProfD, "extensions3");
          userDir = gProfD.clone();
          userDir.append("extensions3");
          userDir.append(gAppInfo.ID);
          Assert.ok(userDir.exists());

          await promiseStartupManager(false);

          AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                       "addon2@tests.mozilla.org"], function([a1_3, a2_3]) {
            Assert.notEqual(a1_3, null);
            Assert.ok(!a1_3.appDisabled);
            Assert.ok(a1_3.isActive);
            Assert.ok(isExtensionInAddonsList(userDir, a1_3.id));

            Assert.notEqual(a2_3, null);
            Assert.ok(!a2_3.appDisabled);
            Assert.ok(a2_3.isActive);
            Assert.ok(!isExtensionInAddonsList(userDir, a2_3.id));
            Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

            executeSoon(run_test_2);
          });
        }));
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

// Set up the profile
function run_test_2() {
  AddonManager.getAddonByID("addon2@tests.mozilla.org", callback_soon(async function(a2) {
   Assert.notEqual(a2, null);
   Assert.ok(!a2.appDisabled);
   Assert.ok(a2.isActive);
   Assert.ok(!isExtensionInAddonsList(userDir, a2.id));
   Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

   a2.userDisabled = true;
   Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

   shutdownManager();

   userDir.parent.moveTo(gProfD, "extensions4");
   userDir = gProfD.clone();
   userDir.append("extensions4");
   userDir.append(gAppInfo.ID);
   Assert.ok(userDir.exists());

   await promiseStartupManager(false);

   AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                "addon2@tests.mozilla.org"], function([a1_2, a2_2]) {
     Assert.notEqual(a1_2, null);
     Assert.ok(!a1_2.appDisabled);
     Assert.ok(a1_2.isActive);
     Assert.ok(isExtensionInAddonsList(userDir, a1_2.id));

     Assert.notEqual(a2_2, null);
     Assert.ok(a2_2.userDisabled);
     Assert.ok(!a2_2.isActive);
     Assert.ok(!isExtensionInAddonsList(userDir, a2_2.id));
     Assert.equal(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

     end_test();
   });
  }));
}
