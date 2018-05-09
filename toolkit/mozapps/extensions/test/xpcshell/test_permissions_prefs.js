/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that xpinstall.[whitelist|blacklist].add preferences are emptied when
// converted into permissions.

const PREF_XPI_WHITELIST_PERMISSIONS  = "xpinstall.whitelist.add";
const PREF_XPI_BLACKLIST_PERMISSIONS  = "xpinstall.blacklist.add";

function newPrincipal(uri) {
  return Services.scriptSecurityManager.createCodebasePrincipal(NetUtil.newURI(uri), {});
}

function do_check_permission_prefs(preferences) {
  // Check preferences were emptied
  for (let pref of preferences) {
    try {
      Assert.equal(Services.prefs.getCharPref(pref), "");
    } catch (e) {
      // Successfully emptied
    }
  }
}

function clear_imported_preferences_cache() {
  let scope = ChromeUtils.import("resource://gre/modules/PermissionsUtils.jsm", {});
  scope.gImportedPrefBranches.clear();
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // Create own preferences to test
  Services.prefs.setCharPref("xpinstall.whitelist.add.EMPTY", "");
  Services.prefs.setCharPref("xpinstall.whitelist.add.TEST", "http://whitelist.example.com");
  Services.prefs.setCharPref("xpinstall.blacklist.add.EMPTY", "");
  Services.prefs.setCharPref("xpinstall.blacklist.add.TEST", "http://blacklist.example.com");

  // Get list of preferences to check
  var whitelistPreferences = Services.prefs.getChildList(PREF_XPI_WHITELIST_PERMISSIONS, {});
  var blacklistPreferences = Services.prefs.getChildList(PREF_XPI_BLACKLIST_PERMISSIONS, {});
  var preferences = whitelistPreferences.concat(blacklistPreferences);

  await promiseStartupManager();

  // Permissions are imported lazily - act as thought we're checking an install,
  // to trigger on-deman importing of the permissions.
  AddonManager.isInstallAllowed("application/x-xpinstall", newPrincipal("http://example.com/file.xpi"));
  do_check_permission_prefs(preferences);


  // Import can also be triggered by an observer notification by any other area
  // of code, such as a permissions management UI.

  // First, request to flush all permissions
  clear_imported_preferences_cache();
  Services.prefs.setCharPref("xpinstall.whitelist.add.TEST2", "https://whitelist2.example.com");
  Services.obs.notifyObservers(null, "flush-pending-permissions", "install");
  do_check_permission_prefs(preferences);

  // Then, request to flush just install permissions
  clear_imported_preferences_cache();
  Services.prefs.setCharPref("xpinstall.whitelist.add.TEST3", "https://whitelist3.example.com");
  Services.obs.notifyObservers(null, "flush-pending-permissions");
  do_check_permission_prefs(preferences);

  // And a request to flush some other permissions sholdn't flush install permissions
  clear_imported_preferences_cache();
  Services.prefs.setCharPref("xpinstall.whitelist.add.TEST4", "https://whitelist4.example.com");
  Services.obs.notifyObservers(null, "flush-pending-permissions", "lolcats");
  Assert.equal(Services.prefs.getCharPref("xpinstall.whitelist.add.TEST4"), "https://whitelist4.example.com");
});
