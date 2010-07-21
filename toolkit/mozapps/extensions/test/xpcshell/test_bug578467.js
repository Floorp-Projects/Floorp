/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that xpinstall.[whitelist|blacklist].add preferences are emptied when
// converted into permissions on startup with new profile

const PREF_XPI_WHITELIST_PERMISSIONS  = "xpinstall.whitelist.add";
const PREF_XPI_BLACKLIST_PERMISSIONS  = "xpinstall.blacklist.add";

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // Create own preferences to test
  Services.prefs.setCharPref("xpinstall.whitelist.add.EMPTY", "");
  Services.prefs.setCharPref("xpinstall.whitelist.add.TEST", "whitelist.example.com");
  Services.prefs.setCharPref("xpinstall.blacklist.add.EMPTY", "");
  Services.prefs.setCharPref("xpinstall.blacklist.add.TEST", "blacklist.example.com");

  // Get list of preferences to check
  var whitelistPreferences = Services.prefs.getChildList(PREF_XPI_WHITELIST_PERMISSIONS, {});
  var blacklistPreferences = Services.prefs.getChildList(PREF_XPI_BLACKLIST_PERMISSIONS, {});
  var preferences = whitelistPreferences.concat(blacklistPreferences);

  startupManager();

  // Check preferences were emptied
  preferences.forEach(function(aPreference) {
    try {
      do_check_eq(Services.prefs.getCharPref(aPreference), "");
    }
    catch (e) {
      // Successfully emptied
    }
  });
}

