/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that PerrmissionsUtils.jsm works as expected, including:
// * PermissionsUtils.importfromPrefs()
//      <ROOT>.[whitelist|blacklist].add preferences are emptied when
//       converted into permissions on startup.


const PREF_ROOT = "testpermissions.";
const TEST_PERM = "text-permission";

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PermissionsUtils.jsm");

function run_test() {
  test_importfromPrefs();
}


function test_importfromPrefs() {
  // Create own preferences to test
  Services.prefs.setCharPref(PREF_ROOT + "whitelist.add.EMPTY", "");
  Services.prefs.setCharPref(PREF_ROOT + "whitelist.add.EMPTY2", ",");
  Services.prefs.setCharPref(PREF_ROOT + "whitelist.add.TEST", "whitelist.example.com");
  Services.prefs.setCharPref(PREF_ROOT + "whitelist.add.TEST2", "whitelist2-1.example.com,whitelist2-2.example.com");
  Services.prefs.setCharPref(PREF_ROOT + "blacklist.add.EMPTY", "");
  Services.prefs.setCharPref(PREF_ROOT + "blacklist.add.TEST", "blacklist.example.com,");
  Services.prefs.setCharPref(PREF_ROOT + "blacklist.add.TEST2", ",blacklist2-1.example.com,blacklist2-2.example.com");

  // Import them
  PermissionsUtils.importFromPrefs(PREF_ROOT, TEST_PERM);

  // Get list of preferences to check
  let preferences = Services.prefs.getChildList(PREF_ROOT, {});

  // Check preferences were emptied
  for (let pref of preferences) {
    do_check_eq(Services.prefs.getCharPref(pref), "");
  }

  // Check they were imported into the permissions manager
  let whitelisted = ["whitelist.example.com",
                     "whitelist2-1.example.com",
                     "whitelist2-2.example.com"];
  let blacklisted = ["blacklist.example.com",
                     "blacklist2-1.example.com",
                     "blacklist2-2.example.com"];
  for (let url of whitelisted) {
    let uri = Services.io.newURI("http://" + url, null, null);
    do_check_eq(Services.perms.testPermission(uri, TEST_PERM), Services.perms.ALLOW_ACTION);
  }
  for (let url of blacklisted) {
    let uri = Services.io.newURI("http://" + url, null, null);
    do_check_eq(Services.perms.testPermission(uri, TEST_PERM), Services.perms.DENY_ACTION);
  }
}
