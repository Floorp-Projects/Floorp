/* exported startup, shutdown, install, uninstall, ADDON_ID, INSTALL_COMPLETE_PREF */
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

const ADDON_ID = "test_delay_update_complete@tests.mozilla.org";
const INSTALL_COMPLETE_PREF = "bootstraptest.install_complete_done";

// global reference to hold upgrade object
let gUpgrade;

function install(data, reason) {}

// normally we would use BootstrapMonitor here, but we need a reference to
// the symbol inside `XPIProvider.jsm`.
function startup(data, reason) {
  // do not apply update immediately, hold on to for later
  if (data.hasOwnProperty("instanceID") && data.instanceID) {
    AddonManager.addUpgradeListener(data.instanceID, (upgrade) => {
      gUpgrade = upgrade;
    });
  } else {
    throw Error("no instanceID passed to bootstrap startup");
  }

  // add a listener so the test can pass control back
  AddonManager.addAddonListener({
    onFakeEvent: () => {
      gUpgrade.install();
    }
  })
}

function shutdown(data, reason) {}

function uninstall(data, reason) {}
