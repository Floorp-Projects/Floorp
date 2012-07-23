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

do_load_httpd_js();
var testserver;

var userDir = gProfD.clone();
userDir.append("extensions2");
userDir.append(gAppInfo.ID);

var dirProvider = {
  getFile: function(aProp, aPersistent) {
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
  updateURL: "http://localhost:4444/data/test_bug655254.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Set up the profile
function run_test() {
  do_test_pending();

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.start(4444);

  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

function run_test_1() {
  var time = Date.now();
  var dir = writeInstallRDFForExtension(addon1, userDir);
  setExtensionModifiedTime(dir, time);

  manuallyInstall(do_get_addon("test_bug655254_2"), userDir, "addon2@tests.mozilla.org");

  startupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a1, a2]) {
    do_check_neq(a1, null);
    do_check_true(a1.appDisabled);
    do_check_false(a1.isActive);
    do_check_false(isExtensionInAddonsList(userDir, a1.id));

    do_check_neq(a2, null);
    do_check_false(a2.appDisabled);
    do_check_true(a2.isActive);
    do_check_false(isExtensionInAddonsList(userDir, a2.id));
    do_check_eq(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

    a1.findUpdates({
      onUpdateFinished: function() {
        restartManager();

        AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
          do_check_neq(a1, null);
          do_check_false(a1.appDisabled);
          do_check_true(a1.isActive);
          do_check_true(isExtensionInAddonsList(userDir, a1.id));

          shutdownManager();

          do_check_eq(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

          userDir.parent.moveTo(gProfD, "extensions3");
          userDir = gProfD.clone();
          userDir.append("extensions3");
          userDir.append(gAppInfo.ID);
          do_check_true(userDir.exists());

          startupManager(false);

          AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                       "addon2@tests.mozilla.org"], function([a1, a2]) {
            do_check_neq(a1, null);
            do_check_false(a1.appDisabled);
            do_check_true(a1.isActive);
            do_check_true(isExtensionInAddonsList(userDir, a1.id));

            do_check_neq(a2, null);
            do_check_false(a2.appDisabled);
            do_check_true(a2.isActive);
            do_check_false(isExtensionInAddonsList(userDir, a2.id));
            do_check_eq(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

            run_test_2();
          });
        });
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

//Set up the profile
function run_test_2() {
  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
   do_check_neq(a2, null);
   do_check_false(a2.appDisabled);
   do_check_true(a2.isActive);
   do_check_false(isExtensionInAddonsList(userDir, a2.id));
   do_check_eq(Services.prefs.getIntPref("bootstraptest.active_version"), 1);

   a2.userDisabled = true;
   do_check_eq(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

   shutdownManager();

   userDir.parent.moveTo(gProfD, "extensions4");
   userDir = gProfD.clone();
   userDir.append("extensions4");
   userDir.append(gAppInfo.ID);
   do_check_true(userDir.exists());

   startupManager(false);

   AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                "addon2@tests.mozilla.org"], function([a1, a2]) {
     do_check_neq(a1, null);
     do_check_false(a1.appDisabled);
     do_check_true(a1.isActive);
     do_check_true(isExtensionInAddonsList(userDir, a1.id));

     do_check_neq(a2, null);
     do_check_true(a2.userDisabled);
     do_check_false(a2.isActive);
     do_check_false(isExtensionInAddonsList(userDir, a2.id));
     do_check_eq(Services.prefs.getIntPref("bootstraptest.active_version"), 0);

     end_test();
   });
  });
}
