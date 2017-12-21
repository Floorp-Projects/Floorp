/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test startup and restart when no add-ons are installed
// bug 944006


// Load XPI Provider to get schema version ID
var XPIScope = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
const DB_SCHEMA = XPIScope.DB_SCHEMA;

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Test for a preference to either exist with a specified value, or not exist at all
function checkPending() {
  try {
    Assert.ok(!Services.prefs.getBoolPref("extensions.pendingOperations"));
  } catch (e) {
    // OK
  }
}

// Make sure all our extension state is empty/nonexistent
function check_empty_state() {
  Assert.equal(Services.prefs.getIntPref("extensions.databaseSchema"), DB_SCHEMA);
  checkPending();
}

// After first run with no add-ons, we expect:
// no extensions.json is created
// no extensions.ini
// database schema version preference is set
// bootstrap add-ons preference is not found
// add-on directory state preference is an empty array
// no pending operations
add_task(async function first_run() {
  startupManager();
  check_empty_state();
  await true;
});

// Now do something that causes a DB load, and re-check
async function trigger_db_load() {
  let addonList = await new Promise(resolve => {
    AddonManager.getAddonsByTypes(["extension"], resolve);
  });

  Assert.equal(addonList.length, 0);
  check_empty_state();

  await true;
}
add_task(trigger_db_load);

// Now restart the manager and check again
add_task(async function restart_and_recheck() {
  restartManager();
  check_empty_state();
  await true;
});

// and reload the DB again
add_task(trigger_db_load);

// When we start up with no DB and an old database schema, we should update the
// schema number but not create a database
add_task(function upgrade_schema_version() {
  shutdownManager();
  Services.prefs.setIntPref("extensions.databaseSchema", 1);

  startupManager();
  Assert.equal(Services.prefs.getIntPref("extensions.databaseSchema"), DB_SCHEMA);
  check_empty_state();
});
