/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that PerrmissionsUtils.jsm works as expected, including:
// * PermissionsUtils.importfromPrefs()
//      <ROOT>.[whitelist|blacklist].add preferences are emptied when
//       converted into permissions on startup.

const PREF_ROOT = "testpermissions.";
const TEST_PERM = "test-permission";

const { PermissionsUtils } = ChromeUtils.import(
  "resource://gre/modules/PermissionsUtils.jsm"
);

function run_test() {
  test_importfromPrefs();
}

function test_importfromPrefs() {
  // Create own preferences to test
  Services.prefs.setCharPref(PREF_ROOT + "whitelist.add.EMPTY", "");
  Services.prefs.setCharPref(PREF_ROOT + "whitelist.add.EMPTY2", ",");
  Services.prefs.setCharPref(
    PREF_ROOT + "whitelist.add.TEST",
    "http://whitelist.example.com"
  );
  Services.prefs.setCharPref(
    PREF_ROOT + "whitelist.add.TEST2",
    "https://whitelist2-1.example.com,http://whitelist2-2.example.com:8080,about:home"
  );
  Services.prefs.setCharPref(
    PREF_ROOT + "whitelist.add.TEST3",
    "whitelist3-1.example.com,about:config"
  ); // legacy style - host only
  Services.prefs.setCharPref(PREF_ROOT + "blacklist.add.EMPTY", "");
  Services.prefs.setCharPref(
    PREF_ROOT + "blacklist.add.TEST",
    "http://blacklist.example.com,"
  );
  Services.prefs.setCharPref(
    PREF_ROOT + "blacklist.add.TEST2",
    ",https://blacklist2-1.example.com,http://blacklist2-2.example.com:8080,about:mozilla"
  );
  Services.prefs.setCharPref(
    PREF_ROOT + "blacklist.add.TEST3",
    "blacklist3-1.example.com,about:preferences"
  ); // legacy style - host only

  // Check they are unknown in the permission manager prior to importing.
  let whitelisted = [
    "http://whitelist.example.com",
    "https://whitelist2-1.example.com",
    "http://whitelist2-2.example.com:8080",
    "http://whitelist3-1.example.com",
    "https://whitelist3-1.example.com",
    "about:config",
    "about:home",
  ];
  let blacklisted = [
    "http://blacklist.example.com",
    "https://blacklist2-1.example.com",
    "http://blacklist2-2.example.com:8080",
    "http://blacklist3-1.example.com",
    "https://blacklist3-1.example.com",
    "about:preferences",
    "about:mozilla",
  ];
  let untouched = [
    "https://whitelist.example.com",
    "https://blacklist.example.com",
    "http://whitelist2-1.example.com",
    "http://blacklist2-1.example.com",
    "https://whitelist2-2.example.com:8080",
    "https://blacklist2-2.example.com:8080",
  ];
  let unknown = whitelisted.concat(blacklisted).concat(untouched);
  for (let url of unknown) {
    let uri = Services.io.newURI(url);
    Assert.equal(
      Services.perms.testPermission(uri, TEST_PERM),
      Services.perms.UNKNOWN_ACTION
    );
  }

  // Import them
  PermissionsUtils.importFromPrefs(PREF_ROOT, TEST_PERM);

  // Get list of preferences to check
  let preferences = Services.prefs.getChildList(PREF_ROOT);

  // Check preferences were emptied
  for (let pref of preferences) {
    Assert.equal(Services.prefs.getCharPref(pref), "");
  }

  // Check they were imported into the permissions manager
  for (let url of whitelisted) {
    let uri = Services.io.newURI(url);
    Assert.equal(
      Services.perms.testPermission(uri, TEST_PERM),
      Services.perms.ALLOW_ACTION
    );
  }
  for (let url of blacklisted) {
    let uri = Services.io.newURI(url);
    Assert.equal(
      Services.perms.testPermission(uri, TEST_PERM),
      Services.perms.DENY_ACTION
    );
  }
  for (let url of untouched) {
    let uri = Services.io.newURI(url);
    Assert.equal(
      Services.perms.testPermission(uri, TEST_PERM),
      Services.perms.UNKNOWN_ACTION
    );
  }
}
