/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that changes that cause an add-on to become unblocked or blocked have
// the right effect

// The tests follow a mostly common pattern. First they start with the add-ons
// unblocked, then they make a change that causes the add-ons to become blocked
// then they make a similar change that keeps the add-ons blocked then they make
// a change that unblocks the add-ons. Some tests skip the initial part and
// start with add-ons detected as blocked.

// softblock1 is enabled/disabled by the blocklist changes so its softDisabled
// property should always match its userDisabled property

// softblock2 gets manually enabled then disabled after it becomes blocked so
// its softDisabled property should never become true after that

// softblock3 does the same as softblock2 however it remains disabled

// softblock4 is disabled while unblocked and so should never have softDisabled
// set to true and stay userDisabled. This add-on is not used in tests that
// start with add-ons blocked as it would be identical to softblock3

// softblock5 is a theme. Currently themes just get disabled when they become
// softblocked and have to be manually re-enabled if they become completely
// unblocked (bug 657520)

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://testing-common/MockRegistrar.jsm");

// Allow insecure updates
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false)

var testserver = createHttpServer();
gPort = testserver.identity.primaryPort;

// register static files with server and interpolate port numbers in them
mapFile("/data/blocklistchange/addon_update1.rdf", testserver);
mapFile("/data/blocklistchange/addon_update2.rdf", testserver);
mapFile("/data/blocklistchange/addon_update3.rdf", testserver);
mapFile("/data/blocklistchange/addon_change.xml", testserver);
mapFile("/data/blocklistchange/app_update.xml", testserver);
mapFile("/data/blocklistchange/blocklist_update1.xml", testserver);
mapFile("/data/blocklistchange/blocklist_update2.xml", testserver);
mapFile("/data/blocklistchange/manual_update.xml", testserver);

testserver.registerDirectory("/addons/", do_get_file("addons"));


var default_theme = {
  id: "default@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  internalName: "classic/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock1_1 = {
  id: "softblock1@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock1_2 = {
  id: "softblock1@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock1_3 = {
  id: "softblock1@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock2_1 = {
  id: "softblock2@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock2_2 = {
  id: "softblock2@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock2_3 = {
  id: "softblock2@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock3_1 = {
  id: "softblock3@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock3_2 = {
  id: "softblock3@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock3_3 = {
  id: "softblock3@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock4_1 = {
  id: "softblock4@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock4_2 = {
  id: "softblock4@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock4_3 = {
  id: "softblock4@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock5_1 = {
  id: "softblock5@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update1.rdf",
  internalName: "test/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock5_2 = {
  id: "softblock5@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update2.rdf",
  internalName: "test/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock5_3 = {
  id: "softblock5@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update3.rdf",
  internalName: "test/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var hardblock_1 = {
  id: "hardblock@tests.mozilla.org",
  version: "1.0",
  name: "Hardblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var hardblock_2 = {
  id: "hardblock@tests.mozilla.org",
  version: "2.0",
  name: "Hardblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var hardblock_3 = {
  id: "hardblock@tests.mozilla.org",
  version: "3.0",
  name: "Hardblocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var regexpblock_1 = {
  id: "regexpblock@tests.mozilla.org",
  version: "1.0",
  name: "RegExp-blocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var regexpblock_2 = {
  id: "regexpblock@tests.mozilla.org",
  version: "2.0",
  name: "RegExp-blocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var regexpblock_3 = {
  id: "regexpblock@tests.mozilla.org",
  version: "3.0",
  name: "RegExp-blocked add-on",
  updateURL: "http://localhost:" + gPort + "/data/blocklistchange/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

const ADDON_IDS = ["softblock1@tests.mozilla.org",
                   "softblock2@tests.mozilla.org",
                   "softblock3@tests.mozilla.org",
                   "softblock4@tests.mozilla.org",
                   "softblock5@tests.mozilla.org",
                   "hardblock@tests.mozilla.org",
                   "regexpblock@tests.mozilla.org"];

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow(parent, url, name, features, openArgs) {
    // Should be called to list the newly blocklisted items
    do_check_eq(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    // Simulate auto-disabling any softblocks
    var list = openArgs.wrappedJSObject.list;
    list.forEach(function(aItem) {
      if (!aItem.blocked)
        aItem.disable = true;
    });

    // run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed", null);

  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

var InstallConfirm = {
  confirm(aWindow, aUrl, aInstalls, aInstallCount) {
    aInstalls.forEach(function(aInstall) {
      aInstall.install();
    });
  },

  QueryInterface(iid) {
    if (iid.equals(Ci.amIWebInstallPrompt)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var InstallConfirmFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return InstallConfirm.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{f0863905-4dde-42e2-991c-2dc8209bc9ca}"),
                          "Fake Install Prompt",
                          "@mozilla.org/addons/web-install-prompt;1", InstallConfirmFactory);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function Pload_blocklist(aFile) {
  let blocklist_updated = new Promise((resolve, reject) => {
    Services.obs.addObserver(function() {
      Services.obs.removeObserver(arguments.callee, "blocklist-updated");

      resolve();
    }, "blocklist-updated");
  });

  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" + gPort + "/data/blocklistchange/" + aFile);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
  return blocklist_updated;
}

// Does a background update check for add-ons and returns a promise that
// resolves when any started installs complete
function Pbackground_update() {
  var installCount = 0;
  var backgroundCheckCompleted = false;

  let updated = new Promise((resolve, reject) => {
    AddonManager.addInstallListener({
      onNewInstall(aInstall) {
        installCount++;
      },

      onInstallEnded(aInstall) {
        installCount--;
        // Wait until all started installs have completed
        if (installCount)
          return;

        AddonManager.removeInstallListener(this);

        // If the background check hasn't yet completed then let that call the
        // callback when it is done
        if (!backgroundCheckCompleted)
          return;

        resolve();
      }
    })

    Services.obs.addObserver(function() {
      Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");
      backgroundCheckCompleted = true;

      // If any new installs have started then we'll call the callback once they
      // are completed
      if (installCount)
        return;

      resolve();
    }, "addons-background-update-complete");
  });

  AddonManagerPrivate.backgroundUpdateCheck();
  return updated;
}

// Manually updates the test add-ons to the given version
function Pmanual_update(aVersion) {
  let Pinstalls = [];
  for (let name of ["soft1", "soft2", "soft3", "soft4", "soft5", "hard1", "regexp1"]) {
    Pinstalls.push(
      AddonManager.getInstallForURL(
        `http://localhost:${gPort}/addons/blocklist_${name}_${aVersion}.xpi`,
        null, "application/x-xpinstall"));
  }

  return Promise.all(Pinstalls).then(installs => {
    let completePromises = [];
    for (let install of installs) {
      completePromises.push(new Promise(resolve => {
        install.addListener({
          onDownloadCancelled: resolve,
          onInstallEnded: resolve
        })
      }));

      AddonManager.installAddonFromAOM(null, null, install);
    }

    return Promise.all(completePromises);
  });
}

// Checks that an add-ons properties match expected values
function check_addon(aAddon, aExpectedVersion, aExpectedUserDisabled,
                     aExpectedSoftDisabled, aExpectedState) {
  do_check_neq(aAddon, null);
  do_print("Testing " + aAddon.id + " version " + aAddon.version + " user "
           + aAddon.userDisabled + " soft " + aAddon.softDisabled
           + " perms " + aAddon.permissions);

  do_check_eq(aAddon.version, aExpectedVersion);
  do_check_eq(aAddon.blocklistState, aExpectedState);
  do_check_eq(aAddon.userDisabled, aExpectedUserDisabled);
  do_check_eq(aAddon.softDisabled, aExpectedSoftDisabled);
  if (aAddon.softDisabled)
    do_check_true(aAddon.userDisabled);

  if (aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED) {
    do_print("blocked, PERM_CAN_ENABLE " + aAddon.id);
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    do_print("blocked, PERM_CAN_DISABLE " + aAddon.id);
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
  } else if (aAddon.userDisabled) {
    do_print("userDisabled, PERM_CAN_ENABLE " + aAddon.id);
    do_check_true(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    do_print("userDisabled, PERM_CAN_DISABLE " + aAddon.id);
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
  } else {
    do_print("other, PERM_CAN_ENABLE " + aAddon.id);
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    if (aAddon.type != "theme") {
      do_print("other, PERM_CAN_DISABLE " + aAddon.id);
      do_check_true(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
    }
  }
  do_check_eq(aAddon.appDisabled, aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED);

  let willBeActive = aAddon.isActive;
  if (hasFlag(aAddon.pendingOperations, AddonManager.PENDING_DISABLE))
    willBeActive = false;
  else if (hasFlag(aAddon.pendingOperations, AddonManager.PENDING_ENABLE))
    willBeActive = true;

  if (aExpectedUserDisabled || aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED) {
    do_check_false(willBeActive);
  } else {
    do_check_true(willBeActive);
  }
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  run_next_test();
}

add_task(function* init() {
  writeInstallRDFForExtension(default_theme, profileDir);
  writeInstallRDFForExtension(softblock1_1, profileDir);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  writeInstallRDFForExtension(softblock5_1, profileDir);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  writeInstallRDFForExtension(regexpblock_1, profileDir);
  startupManager();

  let [/* s1 */, /* s2 */, /* s3 */, s4, s5, /* h, r */] = yield promiseAddonsByIDs(ADDON_IDS);
  s4.userDisabled = true;
  s5.userDisabled = false;
});

// Starts with add-ons unblocked and then switches application versions to
// change add-ons to blocked and back
add_task(function* run_app_update_test() {
  do_print("Test: " + arguments.callee.name);
  yield promiseRestartManager();
  yield Pload_blocklist("app_update.xml");
  yield promiseRestartManager();

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");
});

add_task(function* app_update_step_2() {
  yield promiseRestartManager("2");

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(function* app_update_step_3() {
  yield promiseRestartManager();

  yield promiseRestartManager("2.5");

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");
});

add_task(function* app_update_step_4() {
  yield promiseRestartManager("1");

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s1.userDisabled = false;
  s2.userDisabled = false;
  s5.userDisabled = false;
});

// Starts with add-ons unblocked and then switches application versions to
// change add-ons to blocked and back. A DB schema change is faked to force a
// rebuild when the application version changes
add_task(function* run_app_update_schema_test() {
  do_print("Test: " + arguments.callee.name);
  yield promiseRestartManager();

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");
});

add_task(function* update_schema_2() {
  yield promiseShutdownManager();

  changeXPIDBVersion(100);
  gAppInfo.version = "2";
  startupManager(true);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(function* update_schema_3() {
  yield promiseRestartManager();

  yield promiseShutdownManager();
  changeXPIDBVersion(100);
  gAppInfo.version = "2.5";
  startupManager(true);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");
});

add_task(function* update_schema_4() {
  yield promiseShutdownManager();

  changeXPIDBVersion(100);
  startupManager(false);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");
});

add_task(function* update_schema_5() {
  yield promiseShutdownManager();

  changeXPIDBVersion(100);
  gAppInfo.version = "1";
  startupManager(true);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s1.userDisabled = false;
  s2.userDisabled = false;
  s5.userDisabled = false;
});

// Starts with add-ons unblocked and then loads new blocklists to change add-ons
// to blocked and back again.
add_task(function* run_blocklist_update_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield Pload_blocklist("blocklist_update1.xml");
  yield promiseRestartManager();

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");

  yield Pload_blocklist("blocklist_update2.xml");
  yield promiseRestartManager();

  [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  yield promiseRestartManager();

  yield Pload_blocklist("blocklist_update2.xml");
  yield promiseRestartManager();

  [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  yield Pload_blocklist("blocklist_update1.xml");
  yield promiseRestartManager();

  [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s1.userDisabled = false;
  s2.userDisabled = false;
  s5.userDisabled = false;
});

// Starts with add-ons unblocked and then new versions are installed outside of
// the app to change them to blocked and back again.
add_task(function* run_addon_change_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield Pload_blocklist("addon_change.xml");
  yield promiseRestartManager();

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");
});

add_task(function* run_addon_change_2() {
  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock2_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock3_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock4_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock5_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(hardblock_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(regexpblock_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_2.id), Date.now() + 10000);

  startupManager(false);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(function* run_addon_change_3() {
  yield promiseRestartManager();

  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock2_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock3_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock4_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock5_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(hardblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(regexpblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_3.id), Date.now() + 20000);

  startupManager(false);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");
});

add_task(function* run_addon_change_4() {
  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(softblock5_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(regexpblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_1.id), Date.now() + 30000);

  startupManager(false);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

  s1.userDisabled = false;
  s2.userDisabled = false;
  s5.userDisabled = false;
});

// Starts with add-ons blocked and then new versions are installed outside of
// the app to change them to unblocked.
add_task(function* run_addon_change_2_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);
  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_2, profileDir);
  writeInstallRDFForExtension(softblock2_2, profileDir);
  writeInstallRDFForExtension(softblock3_2, profileDir);
  writeInstallRDFForExtension(softblock4_2, profileDir);
  writeInstallRDFForExtension(softblock5_2, profileDir);
  writeInstallRDFForExtension(hardblock_2, profileDir);
  writeInstallRDFForExtension(regexpblock_2, profileDir);

  startupManager(false);

  let [s1, s2, s3, /* s4 */, /* s5 */, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(function* addon_change_2_test_2() {
  yield promiseRestartManager();

  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock2_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock3_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock4_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock5_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(hardblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(regexpblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_3.id), Date.now() + 10000);

  startupManager(false);

  let [s1, s2, s3, /* s4 */, /* s5 */, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});

add_task(function* addon_change_2_test_3() {
  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock5_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(regexpblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_1.id), Date.now() + 20000);

  startupManager(false);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
  s4.userDisabled = true;
  s5.userDisabled = false;
});

// Add-ons are initially unblocked then attempts to upgrade to blocked versions
// in the background which should fail
add_task(function* run_background_update_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield promiseRestartManager();

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  yield Pbackground_update();
  yield promiseRestartManager();

  [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

// Starts with add-ons blocked and then new versions are detected and installed
// automatically for unblocked versions.
add_task(function* run_background_update_2_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);
  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_3, profileDir);
  writeInstallRDFForExtension(softblock2_3, profileDir);
  writeInstallRDFForExtension(softblock3_3, profileDir);
  writeInstallRDFForExtension(softblock4_3, profileDir);
  writeInstallRDFForExtension(softblock5_3, profileDir);
  writeInstallRDFForExtension(hardblock_3, profileDir);
  writeInstallRDFForExtension(regexpblock_3, profileDir);

  startupManager(false);

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  yield promiseRestartManager();

  yield Pbackground_update();
  yield promiseRestartManager();

  [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
  s4.userDisabled = true;
  s5.userDisabled = true;
});

// Starts with add-ons blocked and then simulates the user upgrading them to
// unblocked versions.
add_task(function* run_manual_update_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield promiseRestartManager();
  yield Pload_blocklist("manual_update.xml");
  yield promiseRestartManager();

  let [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  yield promiseRestartManager();

  yield Pmanual_update("2");
  yield promiseRestartManager();

  [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s5, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  // Can't manually update to a hardblocked add-on
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  yield Pmanual_update("3");
  yield promiseRestartManager();

  [s1, s2, s3, s4, s5, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s5, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

// Starts with add-ons blocked and then new versions are installed outside of
// the app to change them to unblocked.
add_task(function* run_manual_update_2_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);
  yield promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_1, profileDir);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  writeInstallRDFForExtension(softblock5_1, profileDir);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  writeInstallRDFForExtension(regexpblock_1, profileDir);

  startupManager(false);

  let [s1, s2, s3, s4, /* s5 */, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  yield promiseRestartManager();

  yield Pmanual_update("2");
  yield promiseRestartManager();

  [s1, s2, s3, s4, /* s5 */, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  // Can't manually update to a hardblocked add-on
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  yield promiseRestartManager();

  yield Pmanual_update("3");
  yield promiseRestartManager();

  [s1, s2, s3, s4, /* s5 */, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
  s4.userDisabled = true;
});

// Uses the API to install blocked add-ons from the local filesystem
add_task(function* run_local_install_test() {
  do_print("Test: " + arguments.callee.name + "\n");
  yield promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);

  yield promiseInstallAllFiles([
    do_get_file("addons/blocklist_soft1_1.xpi"),
    do_get_file("addons/blocklist_soft2_1.xpi"),
    do_get_file("addons/blocklist_soft3_1.xpi"),
    do_get_file("addons/blocklist_soft4_1.xpi"),
    do_get_file("addons/blocklist_soft5_1.xpi"),
    do_get_file("addons/blocklist_hard1_1.xpi"),
    do_get_file("addons/blocklist_regexp1_1.xpi")
  ]);

  let aInstalls = yield AddonManager.getAllInstalls();
  // Should have finished all installs without needing to restart
  do_check_eq(aInstalls.length, 0);

  let [s1, s2, s3, /* s4 */, /* s5 */, h, r] = yield promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});
