/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const PASS_PREF = "symboltest.instanceid.pass";
const FAIL_BOGUS_PREF = "symboltest.instanceid.fail_bogus";
const FAIL_ID_PREF = "symboltest.instanceid.fail_bogus";
const ADDON_ID = "test_symbol@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

BootstrapMonitor.init();

const ADDONS = {
  test_symbol: {
    "install.rdf": {
      "id": "test_symbol@tests.mozilla.org",
      "name": "Test Symbol",
    },
    "bootstrap.js": String.raw`ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

const PASS_PREF = "symboltest.instanceid.pass";
const FAIL_BOGUS_PREF = "symboltest.instanceid.fail_bogus";
const FAIL_ID_PREF = "symboltest.instanceid.fail_bogus";
const ADDON_ID = "test_symbol@tests.mozilla.org";

// normally we would use BootstrapMonitor here, but we need a reference to
// the symbol inside XPIProvider.jsm.
function startup(data, reason) {
  Services.prefs.setBoolPref(PASS_PREF, false);
  Services.prefs.setBoolPref(FAIL_BOGUS_PREF, false);
  Services.prefs.setBoolPref(FAIL_ID_PREF, false);

  // test with the correct symbol
  if (data.hasOwnProperty("instanceID") && data.instanceID) {
    AddonManager.getAddonByInstanceID(data.instanceID)
      .then(addon => {
        if (addon.id == ADDON_ID) {
          Services.prefs.setBoolPref(PASS_PREF, true);
        }
      }).catch(err => {
        throw Error("no addon found for symbol");
      });

  }

  // test with a totally bogus symbol
  AddonManager.getAddonByInstanceID(Symbol("bad symbol"))
    .then(addon => {
      // there is no symbol by this name, so null should be returned
      if (addon == null) {
        Services.prefs.setBoolPref(FAIL_BOGUS_PREF, true);
      } else {
        throw Error("bad symbol should not match:", addon);
      }
    }).catch(err => {
      throw Error("promise should not have rejected: " + err);
    });

  // try to make a matching symbol - this should fail because it's not a
  // reference to the same symbol stored inside the addons manager.
  AddonManager.getAddonByInstanceID(Symbol(ADDON_ID))
    .then(addon => {
      // there is no symbol by this name, so null should be returned
      if (addon == null) {
        Services.prefs.setBoolPref(FAIL_ID_PREF, true);
      } else {
        throw Error("bad symbol should not match:", addon);
      }
    }).catch(err => {
      throw Error("promise should not have rejected: " + err);
    });

}

function install(data, reason) {}
function shutdown(data, reason) {}
function uninstall(data, reason) {}
`,
  },
};

// symbol is passed when add-on is installed
add_task(async function() {
  await promiseStartupManager();

  PromiseTestUtils.expectUncaughtRejection(/no addon found for symbol/);

  for (let pref of [PASS_PREF, FAIL_BOGUS_PREF, FAIL_ID_PREF])
    Services.prefs.clearUserPref(pref);

  await AddonTestUtils.promiseInstallXPI(ADDONS.test_symbol);

  let addon = await promiseAddonByID(ADDON_ID);

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Symbol");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");

  // most of the test is in bootstrap.js in the addon because BootstrapMonitor
  // currently requires the objects in `data` to be serializable, and we
  // need a real reference to the symbol to test this.
  executeSoon(function() {
    // give the startup time to run
    Assert.ok(Services.prefs.getBoolPref(PASS_PREF));
    Assert.ok(Services.prefs.getBoolPref(FAIL_BOGUS_PREF));
    Assert.ok(Services.prefs.getBoolPref(FAIL_ID_PREF));
  });

  await promiseRestartManager();
});
