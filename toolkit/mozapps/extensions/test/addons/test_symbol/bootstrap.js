/* exported startup, shutdown, install, uninstall, ADDON_ID */
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

const PASS_PREF = "symboltest.instanceid.pass";
const FAIL_BOGUS_PREF = "symboltest.instanceid.fail_bogus";
const FAIL_ID_PREF = "symboltest.instanceid.fail_bogus";
const ADDON_ID = "test_symbol@tests.mozilla.org";

function install(data, reason) {}

// normally we would use BootstrapMonitor here, but we need a reference to
// the symbol inside `XPIProvider.jsm`.
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

function shutdown(data, reason) {}

function uninstall(data, reason) {}
