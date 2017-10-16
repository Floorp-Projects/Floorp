/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const FILE_BLOCKLIST                  = "blocklist.xml";

const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";

const OLD = do_get_file("data/test_overrideblocklist/old.xml");
const NEW = do_get_file("data/test_overrideblocklist/new.xml");
const ANCIENT = do_get_file("data/test_overrideblocklist/ancient.xml");
const OLD_TSTAMP = 1296046918000;
const NEW_TSTAMP = 1396046918000;

const gAppDir = FileUtils.getFile(KEY_APPDIR, []);

var oldAddon = {
  id: "old@tests.mozilla.org",
  version: 1
};
var newAddon = {
  id: "new@tests.mozilla.org",
  version: 1
};
var ancientAddon = {
  id: "ancient@tests.mozilla.org",
  version: 1
};
var invalidAddon = {
  id: "invalid@tests.mozilla.org",
  version: 1
};

function incrementAppVersion() {
  gAppInfo.version = "" + (parseInt(gAppInfo.version) + 1);
}

function clearBlocklists() {
  let blocklist = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
  if (blocklist.exists())
    blocklist.remove(true);

  blocklist = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
  if (blocklist.exists())
    blocklist.remove(true);
}

function reloadBlocklist() {
  Services.prefs.setBoolPref(PREF_BLOCKLIST_ENABLED, false);
  Services.prefs.setBoolPref(PREF_BLOCKLIST_ENABLED, true);
}

function copyToApp(file) {
  file.clone().copyTo(gAppDir, FILE_BLOCKLIST);
}

function copyToProfile(file, tstamp) {
  file = file.clone();
  file.copyTo(gProfD, FILE_BLOCKLIST);
  file = gProfD.clone();
  file.append(FILE_BLOCKLIST);
  file.lastModifiedTime = tstamp;
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  let appBlocklist = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
  if (appBlocklist.exists()) {
    try {
      appBlocklist.moveTo(gAppDir, "blocklist.old");
    } catch (e) {
      todo(false, "Aborting test due to unmovable blocklist file: " + e);
      return;
    }
    do_register_cleanup(function() {
      clearBlocklists();
      appBlocklist.moveTo(gAppDir, FILE_BLOCKLIST);
    });
  }

  run_next_test();
}

// On first run whataver is in the app dir should get copied to the profile
add_test(function test_copy() {
  clearBlocklists();
  copyToApp(OLD);

  incrementAppVersion();
  startupManager();

  reloadBlocklist();
  let blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(AM_Ci.nsIBlocklistService);
  do_check_false(blocklist.isAddonBlocklisted(invalidAddon));
  do_check_false(blocklist.isAddonBlocklisted(ancientAddon));
  do_check_true(blocklist.isAddonBlocklisted(oldAddon));
  do_check_false(blocklist.isAddonBlocklisted(newAddon));

  shutdownManager();

  run_next_test();
});

// An ancient blocklist should be ignored
add_test(function test_ancient() {
  clearBlocklists();
  copyToApp(ANCIENT);
  copyToProfile(OLD, OLD_TSTAMP);

  incrementAppVersion();
  startupManager();

  reloadBlocklist();
  let blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(AM_Ci.nsIBlocklistService);
  do_check_false(blocklist.isAddonBlocklisted(invalidAddon));
  do_check_false(blocklist.isAddonBlocklisted(ancientAddon));
  do_check_true(blocklist.isAddonBlocklisted(oldAddon));
  do_check_false(blocklist.isAddonBlocklisted(newAddon));

  shutdownManager();

  run_next_test();
});

// A new blocklist should override an old blocklist
add_test(function test_override() {
  clearBlocklists();
  copyToApp(NEW);
  copyToProfile(OLD, OLD_TSTAMP);

  incrementAppVersion();
  startupManager();

  reloadBlocklist();
  let blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(AM_Ci.nsIBlocklistService);
  do_check_false(blocklist.isAddonBlocklisted(invalidAddon));
  do_check_false(blocklist.isAddonBlocklisted(ancientAddon));
  do_check_false(blocklist.isAddonBlocklisted(oldAddon));
  do_check_true(blocklist.isAddonBlocklisted(newAddon));

  shutdownManager();

  run_next_test();
});

// An old blocklist shouldn't override a new blocklist
add_test(function test_retain() {
  clearBlocklists();
  copyToApp(OLD);
  copyToProfile(NEW, NEW_TSTAMP);

  incrementAppVersion();
  startupManager();

  reloadBlocklist();
  let blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(AM_Ci.nsIBlocklistService);
  do_check_false(blocklist.isAddonBlocklisted(invalidAddon));
  do_check_false(blocklist.isAddonBlocklisted(ancientAddon));
  do_check_false(blocklist.isAddonBlocklisted(oldAddon));
  do_check_true(blocklist.isAddonBlocklisted(newAddon));

  shutdownManager();

  run_next_test();
});

// A missing blocklist in the profile should still load an app-shipped blocklist
add_test(function test_missing() {
  clearBlocklists();
  copyToApp(OLD);
  copyToProfile(NEW, NEW_TSTAMP);

  incrementAppVersion();
  startupManager();
  shutdownManager();

  let blocklist = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
  blocklist.remove(true);
  startupManager(false);

  reloadBlocklist();
  blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
              getService(AM_Ci.nsIBlocklistService);
  do_check_false(blocklist.isAddonBlocklisted(invalidAddon));
  do_check_false(blocklist.isAddonBlocklisted(ancientAddon));
  do_check_true(blocklist.isAddonBlocklisted(oldAddon));
  do_check_false(blocklist.isAddonBlocklisted(newAddon));

  shutdownManager();

  run_next_test();
});
