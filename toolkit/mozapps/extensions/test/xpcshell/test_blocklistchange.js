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

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");

// Allow insecure updates
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

testserver.registerDirectory("/data/", do_get_file("data"));

const ADDONS = {
  "blocklist_hard1_1": {
    id: "hardblock@tests.mozilla.org",
    version: "1.0",
    name: "Hardblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_hard1_2": {
    id: "hardblock@tests.mozilla.org",
    version: "2.0",
    name: "Hardblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_hard1_3": {
    id: "hardblock@tests.mozilla.org",
    version: "3.0",
    name: "Hardblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_regexp1_1": {
    id: "regexpblock@tests.mozilla.org",
    version: "1.0",
    name: "RegExp-blocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_regexp1_2": {
    id: "regexpblock@tests.mozilla.org",
    version: "2.0",
    name: "RegExp-blocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_regexp1_3": {
    id: "regexpblock@tests.mozilla.org",
    version: "3.0",
    name: "RegExp-blocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft1_1": {
    id: "softblock1@tests.mozilla.org",
    version: "1.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft1_2": {
    id: "softblock1@tests.mozilla.org",
    version: "2.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft1_3": {
    id: "softblock1@tests.mozilla.org",
    version: "3.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft2_1": {
    id: "softblock2@tests.mozilla.org",
    version: "1.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft2_2": {
    id: "softblock2@tests.mozilla.org",
    version: "2.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft2_3": {
    id: "softblock2@tests.mozilla.org",
    version: "3.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft3_1": {
    id: "softblock3@tests.mozilla.org",
    version: "1.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft3_2": {
    id: "softblock3@tests.mozilla.org",
    version: "2.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft3_3": {
    id: "softblock3@tests.mozilla.org",
    version: "3.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft4_1": {
    id: "softblock4@tests.mozilla.org",
    version: "1.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft4_2": {
    id: "softblock4@tests.mozilla.org",
    version: "2.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft4_3": {
    id: "softblock4@tests.mozilla.org",
    version: "3.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft5_1": {
    id: "softblock5@tests.mozilla.org",
    version: "1.0",
    name: "Softblocked add-on",
    bootstrap: true,
    internalName: "test/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft5_2": {
    id: "softblock5@tests.mozilla.org",
    version: "2.0",
    name: "Softblocked add-on",
    bootstrap: true,
    internalName: "test/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
  "blocklist_soft5_3": {
    id: "softblock5@tests.mozilla.org",
    version: "3.0",
    name: "Softblocked add-on",
    bootstrap: true,
    internalName: "test/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"}],
  },
};

const XPIS = {};

for (let [name, manifest] of Object.entries(ADDONS)) {
  XPIS[name] = createTempXPIFile(manifest);
  testserver.registerFile(`/addons/${name}.xpi`, XPIS[name]);
}

var softblock1_1 = {
  id: "softblock1@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update1.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update2.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update3.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update1.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update2.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update3.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update1.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update2.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update3.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update1.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update2.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update3.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update1.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update2.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update3.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update1.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update2.json",
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
  bootstrap: true,
  updateURL: "http://example.com/data/blocklistchange/addon_update3.json",
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
                   "hardblock@tests.mozilla.org",
                   "regexpblock@tests.mozilla.org"];

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow(parent, url, name, features, openArgs) {
    // Should be called to list the newly blocklisted items
    Assert.equal(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    // Simulate auto-disabling any softblocks
    var list = openArgs.wrappedJSObject.list;
    list.forEach(function(aItem) {
      if (!aItem.blocked)
        aItem.disable = true;
    });

    // run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed");

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
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return InstallConfirm.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{f0863905-4dde-42e2-991c-2dc8209bc9ca}"),
                          "Fake Install Prompt",
                          "@mozilla.org/addons/web-install-prompt;1", InstallConfirmFactory);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function Pload_blocklist(aFile) {
  let blocklist_updated = new Promise((resolve, reject) => {
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(observer, "blocklist-updated");

      resolve();
    }, "blocklist-updated");
  });

  Services.prefs.setCharPref("extensions.blocklist.url", "http://example.com/data/blocklistchange/" + aFile);
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
    });

    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(observer, "addons-background-update-complete");
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
  for (let name of ["soft1", "soft2", "soft3", "soft4", "hard1", "regexp1"]) {
    Pinstalls.push(
      AddonManager.getInstallForURL(
        `http://example.com/addons/blocklist_${name}_${aVersion}.xpi`,
        null, "application/x-xpinstall"));
  }

  return Promise.all(Pinstalls).then(installs => {
    let completePromises = [];
    for (let install of installs) {
      completePromises.push(new Promise(resolve => {
        install.addListener({
          onDownloadCancelled: resolve,
          onInstallEnded: resolve
        });
      }));

      AddonManager.installAddonFromAOM(null, null, install);
    }

    return Promise.all(completePromises);
  });
}

// Checks that an add-ons properties match expected values
function check_addon(aAddon, aExpectedVersion, aExpectedUserDisabled,
                     aExpectedSoftDisabled, aExpectedState) {
  Assert.notEqual(aAddon, null);
  info("Testing " + aAddon.id + " version " + aAddon.version + " user "
       + aAddon.userDisabled + " soft " + aAddon.softDisabled
       + " perms " + aAddon.permissions);

  Assert.equal(aAddon.version, aExpectedVersion);
  Assert.equal(aAddon.blocklistState, aExpectedState);
  Assert.equal(aAddon.userDisabled, aExpectedUserDisabled);
  Assert.equal(aAddon.softDisabled, aExpectedSoftDisabled);
  if (aAddon.softDisabled)
    Assert.ok(aAddon.userDisabled);

  if (aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED) {
    info("blocked, PERM_CAN_ENABLE " + aAddon.id);
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    info("blocked, PERM_CAN_DISABLE " + aAddon.id);
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
  } else if (aAddon.userDisabled) {
    info("userDisabled, PERM_CAN_ENABLE " + aAddon.id);
    Assert.ok(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    info("userDisabled, PERM_CAN_DISABLE " + aAddon.id);
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
  } else {
    info("other, PERM_CAN_ENABLE " + aAddon.id);
    Assert.ok(!hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    if (aAddon.type != "theme") {
      info("other, PERM_CAN_DISABLE " + aAddon.id);
      Assert.ok(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
    }
  }
  Assert.equal(aAddon.appDisabled, aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED);

  let willBeActive = aAddon.isActive;
  if (hasFlag(aAddon.pendingOperations, AddonManager.PENDING_DISABLE))
    willBeActive = false;
  else if (hasFlag(aAddon.pendingOperations, AddonManager.PENDING_ENABLE))
    willBeActive = true;

  if (aExpectedUserDisabled || aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED) {
    Assert.ok(!willBeActive);
  } else {
    Assert.ok(willBeActive);
  }
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  run_next_test();
}

add_task(async function init() {
  writeInstallRDFForExtension(softblock1_1, profileDir);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  writeInstallRDFForExtension(regexpblock_1, profileDir);
  startupManager();

  let [/* s1 */, /* s2 */, /* s3 */, s4, /* h, r */] = await promiseAddonsByIDs(ADDON_IDS);
  s4.userDisabled = true;
});

// Starts with add-ons unblocked and then switches application versions to
// change add-ons to blocked and back
add_task(async function run_app_update_test() {
  await promiseRestartManager();
  await Pload_blocklist("app_update.xml");
  await promiseRestartManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

add_task(async function app_update_step_2() {
  await promiseRestartManager("2");

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(async function app_update_step_3() {
  await promiseRestartManager();

  await promiseRestartManager("2.5");

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});

add_task(async function app_update_step_4() {
  await promiseRestartManager("1");

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
});

// Starts with add-ons unblocked and then switches application versions to
// change add-ons to blocked and back. A DB schema change is faked to force a
// rebuild when the application version changes
add_task(async function run_app_update_schema_test() {
  await promiseRestartManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

add_task(async function update_schema_2() {
  await promiseShutdownManager();

  await changeXPIDBVersion(100);
  gAppInfo.version = "2";
  await promiseStartupManager(true);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(async function update_schema_3() {
  await promiseRestartManager();

  await promiseShutdownManager();
  await changeXPIDBVersion(100);
  gAppInfo.version = "2.5";
  await promiseStartupManager(true);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});

add_task(async function update_schema_4() {
  await promiseShutdownManager();

  await changeXPIDBVersion(100);
  startupManager(false);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});

add_task(async function update_schema_5() {
  await promiseShutdownManager();

  await changeXPIDBVersion(100);
  gAppInfo.version = "1";
  await promiseStartupManager(true);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
});

// Starts with add-ons unblocked and then loads new blocklists to change add-ons
// to blocked and back again.
add_task(async function run_blocklist_update_test() {
  await Pload_blocklist("blocklist_update1.xml");
  await promiseRestartManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await Pload_blocklist("blocklist_update2.xml");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  await promiseRestartManager();

  await Pload_blocklist("blocklist_update2.xml");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await Pload_blocklist("blocklist_update1.xml");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
});

// Starts with add-ons unblocked and then new versions are installed outside of
// the app to change them to blocked and back again.
add_task(async function run_addon_change_test() {
  await Pload_blocklist("addon_change.xml");
  await promiseRestartManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

add_task(async function run_addon_change_2() {
  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock2_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock3_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock4_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(hardblock_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_2.id), Date.now() + 10000);
  writeInstallRDFForExtension(regexpblock_2, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_2.id), Date.now() + 10000);

  startupManager(false);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(async function run_addon_change_3() {
  await promiseRestartManager();

  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock2_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock3_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock4_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(hardblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_3.id), Date.now() + 20000);
  writeInstallRDFForExtension(regexpblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_3.id), Date.now() + 20000);

  startupManager(false);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});

add_task(async function run_addon_change_4() {
  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_1.id), Date.now() + 30000);
  writeInstallRDFForExtension(regexpblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_1.id), Date.now() + 30000);

  startupManager(false);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
});

// Starts with add-ons blocked and then new versions are installed outside of
// the app to change them to unblocked.
add_task(async function run_addon_change_2_test() {
  await promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  await promiseStartupManager(false);
  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_2, profileDir);
  writeInstallRDFForExtension(softblock2_2, profileDir);
  writeInstallRDFForExtension(softblock3_2, profileDir);
  writeInstallRDFForExtension(softblock4_2, profileDir);
  writeInstallRDFForExtension(hardblock_2, profileDir);
  writeInstallRDFForExtension(regexpblock_2, profileDir);

  startupManager(false);

  let [s1, s2, s3, /* s4 */, h, r] = await promiseAddonsByIDs(ADDON_IDS);

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

add_task(async function addon_change_2_test_2() {
  await promiseRestartManager();

  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock2_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock3_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(softblock4_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(hardblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_3.id), Date.now() + 10000);
  writeInstallRDFForExtension(regexpblock_3, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_3.id), Date.now() + 10000);

  startupManager(false);

  let [s1, s2, s3, /* s4 */, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});

add_task(async function addon_change_2_test_3() {
  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_1.id), Date.now() + 20000);
  writeInstallRDFForExtension(regexpblock_1, profileDir);
  setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_1.id), Date.now() + 20000);

  startupManager(false);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
  s4.userDisabled = true;
});

// Add-ons are initially unblocked then attempts to upgrade to blocked versions
// in the background which should fail
add_task(async function run_background_update_test() {
  await promiseRestartManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await Pbackground_update();
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

// Starts with add-ons blocked and then new versions are detected and installed
// automatically for unblocked versions.
add_task(async function run_background_update_2_test() {
  await promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  await promiseStartupManager(false);
  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_3, profileDir);
  writeInstallRDFForExtension(softblock2_3, profileDir);
  writeInstallRDFForExtension(softblock3_3, profileDir);
  writeInstallRDFForExtension(softblock4_3, profileDir);
  writeInstallRDFForExtension(hardblock_3, profileDir);
  writeInstallRDFForExtension(regexpblock_3, profileDir);

  startupManager(false);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

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

  await promiseRestartManager();

  await Pbackground_update();
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  s1.userDisabled = false;
  s2.userDisabled = false;
  s4.userDisabled = true;
});

// Starts with add-ons blocked and then simulates the user upgrading them to
// unblocked versions.
add_task(async function run_manual_update_test() {
  await promiseRestartManager();
  await Pload_blocklist("manual_update.xml");
  await promiseRestartManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  s2.userDisabled = false;
  s2.userDisabled = true;
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  s3.userDisabled = false;
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  await promiseRestartManager();

  await Pmanual_update("2");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  // Can't manually update to a hardblocked add-on
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await Pmanual_update("3");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

// Starts with add-ons blocked and then new versions are installed outside of
// the app to change them to unblocked.
add_task(async function run_manual_update_2_test() {
  await promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  await promiseStartupManager(false);
  await promiseShutdownManager();

  writeInstallRDFForExtension(softblock1_1, profileDir);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  writeInstallRDFForExtension(regexpblock_1, profileDir);

  startupManager(false);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

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
  await promiseRestartManager();

  await Pmanual_update("2");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  // Can't manually update to a hardblocked add-on
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await promiseRestartManager();

  await Pmanual_update("3");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

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
add_task(async function run_local_install_test() {
  await promiseShutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);

  await promiseInstallAllFiles([
    XPIS.blocklist_soft1_1,
    XPIS.blocklist_soft2_1,
    XPIS.blocklist_soft3_1,
    XPIS.blocklist_soft4_1,
    XPIS.blocklist_hard1_1,
    XPIS.blocklist_regexp1_1,
  ]);

  let aInstalls = await AddonManager.getAllInstalls();
  // Should have finished all installs without needing to restart
  Assert.equal(aInstalls.length, 0);

  let [s1, s2, s3, /* s4 */, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});
