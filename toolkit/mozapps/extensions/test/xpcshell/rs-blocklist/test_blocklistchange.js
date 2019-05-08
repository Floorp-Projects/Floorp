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

// Allow insecure updates
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

Services.prefs.setBoolPref("extensions.webextPermissionPrompts", false);

var testserver = createHttpServer({hosts: ["example.com"]});
// Needed for updates:
testserver.registerDirectory("/data/", do_get_file("../data"));

const XPIS = {};

const ADDON_IDS = ["softblock1@tests.mozilla.org",
                   "softblock2@tests.mozilla.org",
                   "softblock3@tests.mozilla.org",
                   "softblock4@tests.mozilla.org",
                   "hardblock@tests.mozilla.org",
                   "regexpblock@tests.mozilla.org"];

const BLOCK_APP = [{
  guid: "xpcshell@tests.mozilla.org",
  maxVersion: "2.*",
  minVersion: "2",
}];

function softBlockApp(id) {
  return {
    guid: `${id}@tests.mozilla.org`,
    versionRange: [{
      severity: "1",
      targetApplication: BLOCK_APP,
    }],
  };
}

function softBlockAddonChange(id) {
  return {
    guid: `${id}@tests.mozilla.org`,
    versionRange: [{
      severity: "1",
      minVersion: "2",
      maxVersion: "3",
    }],
  };
}

function softBlockUpdate2(id) {
  return {
    guid: `${id}@tests.mozilla.org`,
    versionRange: [{severity: "1"}],
  };
}

function softBlockManual(id) {
  return {
    guid: `${id}@tests.mozilla.org`,
    versionRange: [{
      maxVersion: "2",
      minVersion: "1",
      severity: "1",
    }],
  };
}

const BLOCKLIST_DATA = {
  app_update: [
    softBlockApp("softblock1"),
    softBlockApp("softblock2"),
    softBlockApp("softblock3"),
    softBlockApp("softblock4"),
    softBlockApp("softblock5"),
    {
      guid: "hardblock@tests.mozilla.org",
      versionRange: [
        {
          targetApplication: BLOCK_APP,
        },
      ],
    },
    {
      guid: "/^RegExp/",
      versionRange: [
        {
          severity: "1",
          targetApplication: BLOCK_APP,
        },
      ],
    },
    {
      guid: "/^RegExp/i",
      versionRange: [
        {
          targetApplication: BLOCK_APP,
        },
      ],
    },
  ],
  addon_change: [
    softBlockAddonChange("softblock1"),
    softBlockAddonChange("softblock2"),
    softBlockAddonChange("softblock3"),
    softBlockAddonChange("softblock4"),
    softBlockAddonChange("softblock5"),
    {
      guid: "hardblock@tests.mozilla.org",
      versionRange: [
        {
          maxVersion: "3",
          minVersion: "2",
        },
      ],
    },
    {
      _comment: "Two RegExp matches, so test flags work - first shouldn't match.",
      guid: "/^RegExp/",
      versionRange: [
        {
          maxVersion: "3",
          minVersion: "2",
          severity: "1",
        },
      ],
    },
    {
      guid: "/^RegExp/i",
      versionRange: [
        {
          maxVersion: "3",
          minVersion: "2",
          severity: "2",
        },
      ],
    },
  ],
  blocklist_update2: [
    softBlockUpdate2("softblock1"),
    softBlockUpdate2("softblock2"),
    softBlockUpdate2("softblock3"),
    softBlockUpdate2("softblock4"),
    softBlockUpdate2("softblock5"),
    {
      guid: "hardblock@tests.mozilla.org",
      versionRange: [],
    },
    {
      guid: "/^RegExp/",
      versionRange: [{severity: "1"}],
    },
    {
      guid: "/^RegExp/i",
      versionRange: [],
    },
  ],
  manual_update: [
    softBlockManual("softblock1"),
    softBlockManual("softblock2"),
    softBlockManual("softblock3"),
    softBlockManual("softblock4"),
    softBlockManual("softblock5"),
    {
      guid: "hardblock@tests.mozilla.org",
      versionRange: [
        {
          maxVersion: "2",
          minVersion: "1",
        },
      ],
    },
    {
      guid: "/^RegExp/i",
      versionRange: [
        {
          maxVersion: "2",
          minVersion: "1",
        },
      ],
    },
  ],
};

// XXXgijs: according to https://bugzilla.mozilla.org/show_bug.cgi?id=1257565#c111
// this code and the related code in Blocklist.jsm (specific to XML blocklist) is
// dead code and can be removed. See https://bugzilla.mozilla.org/show_bug.cgi?id=1549550 .
//
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

  QueryInterface: ChromeUtils.generateQI(["nsIWindowWatcher"]),
};

MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

var InstallConfirm = {
  confirm(aWindow, aUrl, aInstalls, aInstallCount) {
    aInstalls.forEach(function(aInstall) {
      aInstall.install();
    });
  },

  QueryInterface: ChromeUtils.generateQI(["amIWebInstallPrompt"]),
};

var InstallConfirmFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return InstallConfirm.QueryInterface(iid);
  },
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{f0863905-4dde-42e2-991c-2dc8209bc9ca}"),
                          "Fake Install Prompt",
                          "@mozilla.org/addons/web-install-prompt;1", InstallConfirmFactory);

function Pload_blocklist(aId) {
  return AddonTestUtils.loadBlocklistRawData({extensions: BLOCKLIST_DATA[aId]});
}

// Does a background update check for add-ons and returns a promise that
// resolves when any started installs complete
function Pbackground_update() {
  return new Promise((resolve, reject) => {
    let installCount = 0;
    let backgroundCheckCompleted = false;

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
      },
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

    AddonManagerPrivate.backgroundUpdateCheck();
  });
}

// Manually updates the test add-ons to the given version
function Pmanual_update(aVersion) {
  const names = ["soft1", "soft2", "soft3", "soft4", "hard", "regexp"];
  return Promise.all(names.map(async name => {
    let url = `http://example.com/addons/blocklist_${name}_${aVersion}.xpi`;
    let install = await AddonManager.getInstallForURL(url);

    // installAddonFromAOM() does more checking than install.install().
    // In particular, it will refuse to install an incompatible addon.

    return new Promise(resolve => {
      install.addListener({
        onDownloadCancelled: resolve,
        onInstallEnded: resolve,
      });

      AddonManager.installAddonFromAOM(null, null, install);
    });
  }));
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

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  // pattern used to map ids like softblock1 to soft1
  let pattern = /^(soft|hard|regexp)block([1-9]*)@/;
  for (let id of ADDON_IDS) {
    for (let version of [1, 2, 3]) {
      let match = id.match(pattern);
      let name = `blocklist_${match[1]}${match[2]}_${version}`;

      XPIS[name] = createTempWebExtensionFile({
        manifest: {
          name: "Test",
          version: `${version}.0`,
          applications: {
            gecko: {
              id,
              update_url: `http://example.com/data/blocklistchange/addon_update${version}.json`,
            },
          },
        },
      });

      testserver.registerFile(`/addons/${name}.xpi`, XPIS[name]);
    }
  }

  await promiseStartupManager();

  await promiseInstallFile(XPIS.blocklist_soft1_1);
  await promiseInstallFile(XPIS.blocklist_soft2_1);
  await promiseInstallFile(XPIS.blocklist_soft3_1);
  await promiseInstallFile(XPIS.blocklist_soft4_1);
  await promiseInstallFile(XPIS.blocklist_hard_1);
  await promiseInstallFile(XPIS.blocklist_regexp_1);

  let s4 = await promiseAddonByID("softblock4@tests.mozilla.org");
  await s4.disable();
});

// Starts with add-ons unblocked and then switches application versions to
// change add-ons to blocked and back
add_task(async function run_app_update_test() {
  await Pload_blocklist("app_update");
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

  await s2.enable();
  await s2.disable();
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  await s3.enable();
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

  await s1.enable();
  await s2.enable();
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
  let bsPassBlocklist = ChromeUtils.import("resource://gre/modules/Blocklist.jsm", null);
  Object.defineProperty(bsPassBlocklist, "gAppVersion", {value: "2"});
  await promiseStartupManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await s2.enable();
  await s2.disable();
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  await s3.enable();
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(async function update_schema_3() {
  await promiseRestartManager();

  await promiseShutdownManager();
  await changeXPIDBVersion(100);
  gAppInfo.version = "2.5";
  let bsPassBlocklist = ChromeUtils.import("resource://gre/modules/Blocklist.jsm", null);
  Object.defineProperty(bsPassBlocklist, "gAppVersion", {value: "2.5"});
  await promiseStartupManager();

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
  await promiseStartupManager();

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
  let bsPassBlocklist = ChromeUtils.import("resource://gre/modules/Blocklist.jsm", null);
  Object.defineProperty(bsPassBlocklist, "gAppVersion", {value: "1"});
  await promiseStartupManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await s1.enable();
  await s2.enable();
});

// Starts with add-ons unblocked and then loads new blocklists to change add-ons
// to blocked and back again.
add_task(async function run_blocklist_update_test() {
  await AddonTestUtils.loadBlocklistRawData({extensions: []});
  await promiseRestartManager();

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await Pload_blocklist("blocklist_update2");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await s2.enable();
  await s2.disable();
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  await s3.enable();
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  await promiseRestartManager();

  await Pload_blocklist("blocklist_update2");
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await AddonTestUtils.loadBlocklistRawData({extensions: []});
  await promiseRestartManager();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await s1.enable();
  await s2.enable();
});

// Starts with add-ons unblocked and then new versions are installed outside of
// the app to change them to blocked and back again.
add_task(async function run_addon_change_test() {
  await Pload_blocklist("addon_change");
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
  await promiseInstallFile(XPIS.blocklist_soft1_2);
  await promiseInstallFile(XPIS.blocklist_soft2_2);
  await promiseInstallFile(XPIS.blocklist_soft3_2);
  await promiseInstallFile(XPIS.blocklist_soft4_2);
  await promiseInstallFile(XPIS.blocklist_hard_2);
  await promiseInstallFile(XPIS.blocklist_regexp_2);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await s2.enable();
  await s2.disable();
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  await s3.enable();
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
});

add_task(async function run_addon_change_3() {
  await promiseInstallFile(XPIS.blocklist_soft1_3);
  await promiseInstallFile(XPIS.blocklist_soft2_3);
  await promiseInstallFile(XPIS.blocklist_soft3_3);
  await promiseInstallFile(XPIS.blocklist_soft4_3);
  await promiseInstallFile(XPIS.blocklist_hard_3);
  await promiseInstallFile(XPIS.blocklist_regexp_3);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});

add_task(async function run_addon_change_4() {
  await promiseInstallFile(XPIS.blocklist_soft1_1);
  await promiseInstallFile(XPIS.blocklist_soft2_1);
  await promiseInstallFile(XPIS.blocklist_soft3_1);
  await promiseInstallFile(XPIS.blocklist_soft4_1);
  await promiseInstallFile(XPIS.blocklist_hard_1);
  await promiseInstallFile(XPIS.blocklist_regexp_1);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await s1.enable();
  await s2.enable();
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
  await promiseInstallFile(XPIS.blocklist_soft1_3);
  await promiseInstallFile(XPIS.blocklist_soft2_3);
  await promiseInstallFile(XPIS.blocklist_soft3_3);
  await promiseInstallFile(XPIS.blocklist_soft4_3);
  await promiseInstallFile(XPIS.blocklist_hard_3);
  await promiseInstallFile(XPIS.blocklist_regexp_3);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await s2.enable();
  await s2.disable();
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  await s3.enable();
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  await Pbackground_update();

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await s1.enable();
  await s2.enable();
  await s4.disable();
});

// Starts with add-ons blocked and then simulates the user upgrading them to
// unblocked versions.
add_task(async function run_manual_update_test() {
  await Pload_blocklist("manual_update");

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await s2.enable();
  await s2.disable();
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  await s3.enable();
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  await Pmanual_update("2");

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  // Can't manually update to a hardblocked add-on
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await Pmanual_update("3");

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
  let addons = await promiseAddonsByIDs(ADDON_IDS);
  await Promise.all(addons.map(addon => addon.uninstall()));

  await promiseInstallFile(XPIS.blocklist_soft1_1);
  await promiseInstallFile(XPIS.blocklist_soft2_1);
  await promiseInstallFile(XPIS.blocklist_soft3_1);
  await promiseInstallFile(XPIS.blocklist_soft4_1);
  await promiseInstallFile(XPIS.blocklist_hard_1);
  await promiseInstallFile(XPIS.blocklist_regexp_1);

  let [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

  await s2.enable();
  await s2.disable();
  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  await s3.enable();
  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

  await Pmanual_update("2");

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  // Can't manually update to a hardblocked add-on
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);


  await Pmanual_update("3");

  [s1, s2, s3, s4, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  await s1.enable();
  await s2.enable();
  await s4.disable();
});

// Uses the API to install blocked add-ons from the local filesystem
add_task(async function run_local_install_test() {
  let addons = await promiseAddonsByIDs(ADDON_IDS);
  await Promise.all(addons.map(addon => addon.uninstall()));

  await promiseInstallAllFiles([
    XPIS.blocklist_soft1_1,
    XPIS.blocklist_soft2_1,
    XPIS.blocklist_soft3_1,
    XPIS.blocklist_soft4_1,
    XPIS.blocklist_hard_1,
    XPIS.blocklist_regexp_1,
  ]);

  let installs = await AddonManager.getAllInstalls();
  // Should have finished all installs without needing to restart
  Assert.equal(installs.length, 0);

  let [s1, s2, s3, /* s4 */, h, r] = await promiseAddonsByIDs(ADDON_IDS);

  check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
});
