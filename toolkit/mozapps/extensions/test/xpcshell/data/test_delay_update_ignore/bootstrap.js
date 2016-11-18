/* exported startup, shutdown, install, uninstall, ADDON_ID */
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

const ADDON_ID = "test_delay_update_ignore@tests.mozilla.org";
const TEST_IGNORE_PREF = "delaytest.ignore";

function install(data, reason) {}

// normally we would use BootstrapMonitor here, but we need a reference to
// the symbol inside `XPIProvider.jsm`.
function startup(data, reason) {
  Services.prefs.setBoolPref(TEST_IGNORE_PREF, false);

  // explicitly ignore update, will be queued for next restart
  if (data.hasOwnProperty("instanceID") && data.instanceID) {
    AddonManager.addUpgradeListener(data.instanceID, (upgrade) => {
      Services.prefs.setBoolPref(TEST_IGNORE_PREF, true);
    });
  } else {
    throw Error("no instanceID passed to bootstrap startup");
  }
}

function shutdown(data, reason) {}

function uninstall(data, reason) {}
